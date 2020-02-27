//
// Created by 徐颖 on 2/17/20.
//

#ifndef PROXY_PROXY_H
#define PROXY_PROXY_H

#include <sstream>
#include "request.h"
#include "cache.h"
#include <fstream>
#include <sys/socket.h>
#define MYLOG "/var/log/erss/proxy.log"
const int buffer_size = 65535;

class Proxy {
private:
    Request* request;
    std::map<std::string, std::string> *req_header;

    int proxy_id;
    int client_fd;
    int server_fd;
    int content_length;
    ofstream log_flow;

    Response* response_back; // cached
    Response* response_refetched;
    std::map<std::string, std::string> *resp_header;
    vector<vector<char>> resp_buffer;
    vector<vector<char>> req_buffer;
    void get_request_from_client();
    void my_recv_request(int sender_fd, vector<vector<char>>& buf);
    void connect_request(); // if request.method == "CONNECT"
    bool connect_with_server();
    void send_ack();
    void data_forwarding();
    void re_fetching();
    void update_cache();
    bool reply_to_client(Response* res);
    void get_request(); // if request.method == "GET"
    time_t get_time(string date_or_exp);
    time_t get_expiration_time();
    bool revalidation();
    bool can_update();
    size_t my_recv(int fd);
    size_t my_send(int fd, vector<vector<char>>& buf);
    size_t continue_recv(int fd);
    void post_request(); // if request.method == "POST"
public:
    Proxy() {}
    Proxy(int client_fd, int id):
    proxy_id(id),
    client_fd(client_fd),
    server_fd(0),
    content_length(0),
    request(nullptr),
    response_back(nullptr),
    response_refetched(nullptr),
    req_header(nullptr),
    resp_header(nullptr){}
    ~Proxy() {
//        free(request);
//        free(response_back);
    }

    void proxy_run() {
        get_request_from_client();
        if (request->get_method() == "GET" ) {
            get_request();
            std::cout << "================ " << proxy_id << ": GET-END ================" << std::endl;
        }
        else if (request->get_method() == "CONNECT") {
            connect_request();
            std::cout << "================ " << proxy_id << ": CONNECT-END================" << std::endl;
        }
        else if (request->get_method() == "POST") {
            post_request();
            std::cout << "================ " << proxy_id << ": POST-END================" << std::endl;
        }
        close(client_fd);
        close(server_fd);
        return;
    }
};

void Proxy::send_ack() {
    const char *ack = "HTTP/1.1 200 OK\r\n\r\n";
    int status = send(client_fd, ack, strlen(ack), 0);
    if (status == -1) {
        cerr << proxy_id << ": Error: cannot send 200 okay to client" << endl;
        return;
    }
}

void Proxy::data_forwarding() {
    std::vector<char> buffer(buffer_size);
    char *p = buffer.data();
    fd_set readfds; // list listening on
    int fdmax = -1;
    fdmax = (server_fd > client_fd)? server_fd: client_fd;

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(client_fd, &readfds);
        int status = select(fdmax + 1, &readfds, nullptr, nullptr, nullptr);
        if (status <= 0) {
            cerr << proxy_id << ": ERORR" << endl;
            break;
        }
        if (FD_ISSET(server_fd, &readfds)) {
            // recv data from server
            status = recv(server_fd, p, buffer_size, 0);
            if (status < 0) {
                cout << proxy_id << ": status < 0" << endl;
                break;
            } else if (status == 0) {
                cerr << proxy_id << ": Server has closed Tunnel." << endl;
                close(server_fd);
                return;
            } else {
                // forward data to client
                status = send(client_fd, p, status, 0);
                if (status == -1) {
                    break;
                }
            }
        } else if (FD_ISSET(client_fd, &readfds)) {
            // recv data from client
            status = recv(client_fd, p, buffer_size, 0);
            if (status < 0) {
                cout << proxy_id << ": client status < 0" << endl;
                break;
            } else if (status == 0) {
                cerr << proxy_id << ": Client has closed Tunnel." << endl;
                close(server_fd);
                return;
            } else {
                // forward data to server
                status = send(server_fd, p, status, 0);
                if (status == -1) {
                    break;
                }
            }
        }
    } // while
}

void Proxy::my_recv_request(int sender_fd, vector<vector<char>> &buffer) {
    vector<char> buf(buffer_size);
    char *p = buf.data();
    int len = 0;
    len = recv(sender_fd, p, buffer_size, 0);
    if (len < 0) {
        cerr << proxy_id << ": Receive Request Failure" << endl;
        return;
    } else if (len == 0) {
        cerr << proxy_id << ": Client sent 0" << endl;
        return;
    }
    buffer.push_back(buf);
    string cur_string(buf.begin(), buf.end());
    int total = len;
    string header = cur_string.substr(0, cur_string.find("\r\n\r\n") + 4);
    request = new Request(header, proxy_id, &buffer);
    while(true){
        if (total == request->get_content_len() + request->get_header_len()) {
            break;
        }
        size_t size = request->get_content_len() + request->get_header_len() - total;
        vector<char> once_buf(size);
        char *once_p = once_buf.data();
        len = recv(client_fd, once_p, size, 0);
        if (len <= 0) {
            break;
        }
        buffer.push_back(once_buf);
        total += len;
    }
//    cout << proxy_id << ": Received " << total << " bytes from client" << endl;
}

void Proxy::get_request_from_client() {
    // get request from client
    my_recv_request(client_fd, req_buffer);
    req_header = request->get_header();
    cout << endl << "================= " << proxy_id << ": " << request->get_method() << " Request Received =================" << endl;
    cout << proxy_id << ": " << request->get_request_line() << endl;
}

void Proxy::connect_request() {
    // connect with server
    if (connect_with_server() == false) {
        return;
    }
    // send 200 OK back to client
    send_ack();
    // data transfer
    try{
        data_forwarding();
    } catch (std::exception &e) {
        cerr << proxy_id << ": Data Forwarding Failure." << endl;
        close(server_fd);
        return;
    }
    close(server_fd);
    return;
}

bool Proxy::connect_with_server() {
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    int status;
    memset(&host_info, 0, sizeof(host_info));//make sure the struct is empty
    host_info.ai_family   = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
//    const char* hostname = request->get_host().c_str();
    const char* port = "";
    if (request->get_port() == "") {
        port = "80";
    } else {
        port = request->get_port().c_str();
    }

    status = getaddrinfo(request->get_host().c_str(),
            port,
            &host_info,
            &host_info_list);
    if (status != 0) {
        cerr << proxy_id << ": Error: cannot get address info for host" << endl;
        return false;
    }
    server_fd = socket(host_info_list->ai_family,
                              host_info_list->ai_socktype,
                              host_info_list->ai_protocol);
    if (server_fd == -1) {
        cerr << proxy_id << ": Error: cannot create socket" << endl;
        return false;
    }
    /* connect */
//    std::cout << "get_server_addr: " << host_info_list->ai_addr << std::endl;
    status = connect(server_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        cerr << proxy_id << ": ";
        perror("Error: cannot connect to socket");
        return false;
    }
    cout << proxy_id << ": Connect server successfully!" << endl;
    return true;
}

void Proxy::get_request(){
    string url = request->get_full_url();
    Response *cached_resp = cache.get(url);
    /*
         * Mi Yi please take care of this part
         *
         * if expired, refetching
         * if not expired, check cache-control
         *
         * request:
         * if cache-control -> no cache -> revalidation
         * if cache-control -> no store -> re-fetching
         * response:
         * if cache-control -> no cache/must-revalidate/proxy-revalidate -> revalidation
         * if valid and not expire, send back to client.
         *
         */
    if (cached_resp != nullptr) {
        // check if expire
        time_t expired_time = get_expiration_time();
        time_t now = time(0);
        if (difftime(now, expired_time)>0) {
            std::cout << "In cache, but expired at " << asctime(gmtime(&expired_time)) << std::endl;
            // refetching
            re_fetching();
            // update cache
            if (can_update()) {
                cache.put(request->get_full_url(), response_refetched);
            }
            if(my_send(client_fd, *response_refetched->get_buffer()) < 0){
                cerr << "Error: reply to client" << endl;
            }
            cout << "=========" << proxy_id << ": Successfully Reply ========" << endl;
            return;
        }
        else {// if not expired
            // request cache-control
            auto req_it = req_header->find("Cache-Control");
            if (req_it != req_header->end()) {
                // Found Request Cache-Control
                string request_cache_control = req_it->second;
                // no-cahce -> revalidate
                if (request_cache_control.find("no-cache") != string::npos) {
                    // revalidate
                    std::cout << "in cache, but need to revalidate" << std::endl;
                    bool valid = revalidation();
                    if (valid) {
                        std::cout << "in cache, revalidation succeed, valid" << std::endl;
                        // =======================================
                        if(my_send(client_fd, *cached_resp->get_buffer()) < 0){
                            cerr << "Error: reply to client" << endl;
                        }
                        cout << "=========" << proxy_id << ": Successfully Reply ========" << endl;
                        // TBD ===================================
                        return;
                    }
                    else {
                        std::cout << "in cache, revalidation failed, refetching" << std::endl;
                        // refetching
                        re_fetching();
                        // update
                        if (can_update()) {
                            cache.put(request->get_full_url(), response_refetched);
                        }
                        if(my_send(client_fd, *response_refetched->get_buffer()) < 0){
                            cerr << "Error: reply to client" << endl;
                        }
                        cout << "=========" << proxy_id << ": Successfully Reply ========" << endl;
                        return;
                    }

                }
                // no-store -> refetching
                if (req_header->find("no-store")!=req_header->end()){
                    std::cout << "in cache, but request saying no-store, refetching" << std::endl;
                    // refetching
                    re_fetching();
                    // update
                    if (can_update()) {
                        cache.put(request->get_full_url(), response_refetched);
                    }
                    if(my_send(client_fd, *response_refetched->get_buffer()) < 0){
                        cerr << "Error: reply to client" << endl;
                    }
                    cout << "=========" << proxy_id << ": Successdully Reply ========" << endl;
                    return;
                }
            }
            // response cache-control
            std::map<string, string> *cached_resp_header = cached_resp->get_header();
            auto resp_it = cached_resp_header->find("Cache-Control");
            if (resp_it != cached_resp_header->end()) {
                string response_cached_cache_control = resp_it->second;
                if (response_cached_cache_control.find("no-cache") != string::npos ||
                    response_cached_cache_control.find("must-revalidate") != string::npos ||
                    response_cached_cache_control.find("proxy-revalidate") != string::npos ) {
                    std::cout << "in cache, but need to revalidate" << std::endl;
                    bool valid = revalidation();
                    if (valid) {
                        std::cout << "in cache, revalidation succeed, valid" << std::endl;
                        // =============================================
                        if(my_send(client_fd, *cached_resp->get_buffer()) < 0){
                            cerr << "Error: reply to client" << endl;
                        }
                        cout << "=========" << proxy_id << ": Successdully Reply ========" << endl;
                        // ==============================================
                        return;
                    }
                    else {
                        std::cout << "in cache, revalidation failed, refetching" << std::endl;
                        // refetching
                        re_fetching();
                        // update
                        if (can_update()) {
                            cache.put(request->get_full_url(), response_refetched);
                        }
                        if(my_send(client_fd, *response_refetched->get_buffer()) < 0){
                            cerr << "Error: reply to client" << endl;
                        }
                        cout << "=========" << proxy_id << ": Successdully Reply ========" << endl;
                        return;
                    }
                }
            }
            std::cout << "in cache, valid" << std::endl;
            if(!reply_to_client(cached_resp)){
                cerr << "Error: reply to client" << endl;
            }
        }
        cout << "Found and reply to client successfully" << endl;
    } else {
        // Not found! Refetching! Get Response from Server
        cout << proxy_id << ": Not found! Refetching! Get Response from Server" << endl;
        re_fetching();
        // Update or Insert Cache -> update_cache()
        // Need more details like check no store, must revalidation...
        if (can_update()) {
            cache.put(request->get_full_url(), response_back);
        }
        // Reply to Client
        if(my_send(client_fd, *response_refetched->get_buffer()) < 0){
            cerr << "Error: reply to client" << endl;
        }
        cout << "=========" << proxy_id << ": Successdully Reply ========" << endl;
    }
}

bool Proxy::reply_to_client(Response* res){
    int status = send(client_fd, res->get_response().c_str(),
                      res->get_response().length(), 0);
    if (status == -1) {
        cerr << proxy_id << ": Error: cannot send response back to client" << endl;
        return false;
    }
    cout << "=========" << proxy_id << ": successfully answer the client ==========" << endl;
    close(client_fd);
    return true;
}

void Proxy::re_fetching(){
    if (!connect_with_server()) {
        return;
    }
    int status = send(server_fd, request->get_request().c_str(), request->get_request().length(), 0);
    if (status == -1) {
        // log
        cerr << proxy_id << ": Error: cannot send request to server" << endl;
        close(server_fd);
        return;
    }
    cout << "=========== " << proxy_id << ": Sent Request Successfully in re_fetching =========" << endl;
    size_t len = my_recv(server_fd);
    if (len < 0) {
        cerr << "loop recv failure" << endl;
        return;
    } else if (len == 0) {
        cerr << "close tunnel" << endl;
        return;
    }
    cout << "=========== " << proxy_id << ": Received " << resp_buffer.size() << " From Server =========" << endl;
}

void Proxy::post_request() {
    re_fetching();
    if(my_send(client_fd, *response_refetched->get_buffer()) < 0){
        cerr << "Error: reply to client" << endl;
    }
    cout << "=========" << proxy_id << ": Successdully Reply ========" << endl;
    close(client_fd);
}

bool Proxy::revalidation(){
    // construct revalidation request
    int status;
    Response *cached_resp = cache.get(request->get_full_url());
    if (cached_resp == nullptr) {
        cerr << "Error: Cache Not Found" << endl;
        return false;
    }
    std::map<std::string, std::string> *cached_resp_header = cached_resp->get_header();
    std::string newheader = "";
    auto it_etag = cached_resp_header->find("Etag");
    if (it_etag != cached_resp_header->end()) {
        newheader += string("\r\n") + string("If-None-Match:") + it_etag->second;
    }
    else {
        auto it_mod = cached_resp_header->find("Last-Modified");
        if (it_mod != cached_resp_header->end()) {
            newheader += string("\r\n")+ string("If-Modified-Since:") + it_mod->second;
        }
        else {
            return false;
        }
    }
    string req = request->get_request();
    string req_content = req.substr(0, req.find("\r\n\r\n")+4);
    req_content.insert(req_content.find("\r\n\r\n"), newheader);
    //send revalidation
    status = send(server_fd, req_content.c_str(), req_content.length(), 0);
    if (status < 0) {
        return false;
    }
    std::vector<char> buffer(999);
    char* p = buffer.data();
    status = recv(server_fd, p, 999, 0);
    if (status < 0) {
        return false;
    }
    std::string s(p);

    // identify code
    string code = s.substr(s.find(" ") + 1, 3);
    if (code == "200") {
        return true;
    }
    return false;
}

bool Proxy::can_update() {
    std::map<std::string, std::string> *header = response_refetched->get_header();
    auto cc = header->find("Cache-Control");
    auto exp = header->find("Expires");
    auto date = header->find("Date");
    auto age = header->find("Age");

    std::string cache_control;

    if (cc != header->end()){
        cache_control = cc->second;
        if (cache_control.find("no-store") != string::npos ||
            cache_control.find("private") != string::npos ||
            cache_control.find("Authorization") != string::npos){
            return false;
        }
        if(cache_control.find("max-age")!=string::npos) {
            if (date == header->end() && age == header->end() && exp == header->end()) {
                return false;
            }
        } else {
            if (exp == header->end()){
                return false;
            }
        }
    }else{
        if(exp == header->end()){
            return false;
        }
    }
    return true;
}

time_t Proxy::get_time(string date_or_exp) {
    int n = date_or_exp.length();
    char char_array[n + 1];
    strcpy(char_array, date_or_exp.c_str());
    const char *time_details = char_array;
    struct tm tm;
    strptime(time_details, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    time_t t = mktime(&tm);
    return t;
}

time_t Proxy::get_expiration_time(){
    std::map<std::string, std::string> *header = response_back->get_header();
    auto cc = header->find("Cache-Control");
    auto exp = header->find("Expires");
    time_t now = time(0);
    // check expire
    if(exp != header->end()){
        time_t expire_time = get_time((exp->second).substr(1));
        return expire_time;
    }
        // check max-age
    else if(cc != header->end()){
        auto date = header->find("Date");
        auto age = header->find("Age");
        std::string cache_control = cc->second;
        std::string max_age = cache_control.substr(cache_control.find("max-age=") + 8, cache_control.find("max-age=") + 16);
        if (max_age.find(",") != string::npos){
            max_age = max_age.substr(0, max_age.find(","));
        }
        time_t ma_time = (time_t)atoi(max_age.c_str());
        if (date != header->end()){
            time_t date_time = get_time((date->second).substr(1));
            return date_time + ma_time;
        }else if (age != header->end()) {
            string age_str = age->second;
            time_t age_time = (time_t)atoi(age_str.c_str());
            return now - (age_time - ma_time);
        }
    }
    return 0;
}

size_t Proxy::my_recv(int fd){
    vector<char> first_buf(65535);
    char *p = first_buf.data();
    size_t len = 0;
    if((len = recv(fd, p, 65535, 0))<=0){
        return len;
    }
    resp_buffer.push_back(first_buf);
    // mybuffer is the buffer taking the whole response

    string first_str(first_buf.begin(), first_buf.end());
    // get header
    string header_str = first_str.substr(0, first_str.find("\r\n\r\n") + 4);
    response_refetched = new Response(header_str.c_str(), &resp_buffer);
    if(response_refetched->if_chunked()) {
        // chunked
        while(true) {
            size_t once_len;
            if((once_len = continue_recv(fd)) <= 0) {
                return len;
            }
            len += once_len;
            vector<char> last_buf = resp_buffer[resp_buffer.size()-1];
            if (strstr(last_buf.data(), "0\r\n\r\n") != nullptr) {
                break;
            }
        }
        return len;
    }
    else if(response_refetched->if_content_length()) {
        string content = header_str.substr(header_str.find("Content-Length: "));
        content = content.substr(16, content.find("\r\n"));
        size_t total = (size_t)atoi(content.c_str());
        // if cur_len < total_content_length + header_size
        while (len < total + header_str.size()){
            size_t once_len = 0;
            if((once_len = continue_recv(fd)) <= 0){
                return len;
            }
            len += once_len;
        }
        return len;
    }
    return 0;
}

size_t Proxy::continue_recv(int fd) {
    vector<char> continue_buf(65535);
    char *p = continue_buf.data();
    size_t once_len = 0;
    if((once_len = recv(fd, p, 65535, 0)) <=0 ){
        return once_len;
    }
    resp_buffer.push_back(continue_buf);
    return once_len;
}

size_t Proxy::my_send(int fd, vector<vector<char>>& buf) {
    size_t len = 0;
    int sent = 0;
    while(sent != buf.size()) {
        //int once_len = send(fd, mybuffer[i].data(), mybuffer[i].size(), 0));
        // MSG_NOSIGNAL not found;
        int once_len = send(fd, buf[sent].data(), buf[sent].size(), 0);
        if (once_len <=0){
            return once_len;
        }
        len += once_len;
        sent += 1;
    }
    return len;
}

#endif //PROXY_PROXY_H
