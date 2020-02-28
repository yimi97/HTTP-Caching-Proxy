//
// Created by 徐颖 on 2/17/20.
//

/*
    send cached response: error handler
*/

#ifndef PROXY_PROXY_H
#define PROXY_PROXY_H
#include <ctime>
#include <iostream>
#include <locale>
#include <iomanip>
#include <sstream>
#include <time.h>
#include <typeinfo>
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
    vector<vector<char>> my_buffer;
    void get_request_from_client();
    void my_recv_request(int sender_fd, vector<vector<char>>& buf);
    void send_cached_response();
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
    size_t my_recv(int fd, vector<vector<char>> &mybuffer);
    size_t my_send(int fd, vector<vector<char>> &buf);
    size_t continue_recv(int fd);
    void post_request(); // if request.method == "POST"
    /* 
     * new function
     */
    bool can_get_expinfo();
    void resp_to_buf(vector<vector<char>> &mybuffer, string response);

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
    }

    void proxy_run() {
        get_request_from_client();
        if (request == nullptr) {
            cerr << proxy_id << ": Did not receive any request ENC================" << endl;
            return;
        }
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

void Proxy::get_request_from_client() {
    // get request from client
    my_buffer.clear();
    int len = my_recv(client_fd, my_buffer);
    if (len <= 0) {
        cerr << "my_recv Failure" << endl;
        return;
    }
    string str = "";
    int i = 0;
    while(i < my_buffer.size()){
        str += my_buffer[i].data();
        i++;
    }
    request = new Request(str, proxy_id);
    req_header = request->get_header();
    cout << endl << "================= " << proxy_id << ": " << request->get_method() << " Request Received =================" << endl;
    cout << proxy_id << ": " << request->get_request_line() << endl;
    cout << proxy_id << ": " << request->get_port() << endl;
    cout << proxy_id << ": " << request->get_host() << endl;
}

void Proxy::connect_request() {
    // connect with server
    if (!connect_with_server()) {
        cerr << "Connect Failure" << endl;
        return;
    }
    // send 200 OK back to client
    send_ack();
    // data transfer
    try{
        data_forwarding();
    } catch (std::exception &e) {
        cerr << proxy_id << ": Data Forwarding Failure." << endl;
        return;
    }
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


    status = getaddrinfo(request->get_host().c_str(),
                         request->get_port().c_str(),
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
    response_back = cached_resp;
    cout << "Hi!!! I am in GET!!!!" << endl;
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
        cout << proxy_id << ": Hi!!! Found in Cache!!!!" << endl;
//        cout << proxy_id << ": url: " << url << endl;

        if(!can_get_expinfo()){
            std::cout << proxy_id << ": in cache, cannot get expinfo, need to revalidate" << std::endl;
            bool valid = revalidation();
            if (valid) {
                std::cout << proxy_id << ": in cache, cannot get expinfo, revalidation succeed, valid" << std::endl;
                // =======================================
                send_cached_response();
                cout << "=========" << proxy_id << ": Successfully Reply ========" << endl;
                // TBD ===================================
                return;
            }
            else {
                std::cout << proxy_id << ": in cache, cannot get expinfo, revalidation failed, refetching" << std::endl;
                // refetching
                re_fetching();
                // update
                if (can_update()) {
                    cache.put(request->get_full_url(), response_refetched);
                }
                if(my_send(client_fd, my_buffer) < 0){
                    cerr << proxy_id << ": Error: reply to client" << endl;
                    return;
                }
                cout << "=========" << proxy_id << ": Successfully Reply ========" << endl;
                return;
            }
        }

        time_t expired_time = get_expiration_time();
        time_t now = time(0);
        std::cout << proxy_id << ": In cache, expiration time " << asctime(gmtime(&expired_time)) << std::endl;
        if (difftime(now, expired_time)>0) {
            std::cout << proxy_id << ": In cache, but expired at " << asctime(gmtime(&expired_time)) << std::endl;
            // refetching
            re_fetching();
            // update cache
            if (can_update()) {
                cache.put(request->get_full_url(), response_refetched);
            }
            if(my_send(client_fd, my_buffer) < 0){
                cerr << proxy_id << ": Error: reply to client" << endl;
                return;
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
                    std::cout << proxy_id << ": in cache, but need to revalidate" << std::endl;
                    bool valid = revalidation();
                    if (valid) {
                        std::cout << proxy_id << ": in cache, revalidation succeed, valid" << std::endl;
                        // =======================================
                        send_cached_response();
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
                        if(my_send(client_fd, my_buffer) < 0){
                            cerr << proxy_id << ": Error: reply to client" << endl;
                            return;
                        }
                        cout << "=========" << proxy_id << ": Successfully Reply ========" << endl;
                        return;
                    }

                }
                // no-store -> refetching
                if (req_header->find("no-store")!=req_header->end()){
                    std::cout << proxy_id << ": in cache, but request saying no-store, refetching" << std::endl;
                    // refetching
                    re_fetching();
                    // update
                    if (can_update()) {
                        cache.put(request->get_full_url(), response_refetched);
                    }
                    if(my_send(client_fd, my_buffer) < 0){
                        cerr << proxy_id << ": Error: reply to client" << endl;
                        return;
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
                    std::cout << proxy_id << ": in cache, but need to revalidate" << std::endl;
                    bool valid = revalidation();
                    if (valid) {
                        std::cout << proxy_id << ": in cache, revalidation succeed, valid" << std::endl;
                        // =============================================
                        send_cached_response();
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
                        if(my_send(client_fd, my_buffer) < 0){
                            cerr << proxy_id << ": Error: reply to client" << endl;
                            return;
                        }
                        cout << "=========" << proxy_id << ": Successdully Reply ========" << endl;
                        return;
                    }
                }
            }
            std::cout << "in cache, valid" << std::endl;
            send_cached_response();
        }
        cout << "Found and reply to client successfully" << endl;
    } else {
        // Not found! Refetching! Get Response from Server
        cout << proxy_id << ": Not found! Refetching! Get Response from Server" << endl;
        re_fetching();
        // Update or Insert Cache -> update_cache()
        // Need more details like check no store, must revalidation...
        if (can_update()) {
            cache.put(request->get_full_url(), response_refetched);
            cout << proxy_id << ": Successfully Update Cache!" << endl;
        }
        // Reply to Client
        if(my_send(client_fd, my_buffer) <= 0){
            cerr << proxy_id << ": Error: reply to client" << endl;
            return;
        }
        cout << "=========" << proxy_id << ": Successdully Reply ========" << endl;
    }
}

//bool Proxy::reply_to_client(Response* res){
//    int status = my_send(client_fd, res->get_response().c_str(),
//                      res->get_response().length(), 0);
//    if (status == -1) {
//        cerr << proxy_id << ": Error: cannot send response back to client" << endl;
//        return false;
//    }
//    cout << "=========" << proxy_id << ": successfully answer the client ==========" << endl;
//    return true;
//}

void Proxy::re_fetching(){
    if (!connect_with_server()) {
        return;
    }
    int status = my_send(server_fd, my_buffer);
    if (status == -1) {
        // log
        cerr << proxy_id << ": Error: cannot send request to server" << endl;
        return;
    }
    cout << "=========== " << proxy_id << ": Sent Request Successfully in re_fetching =========" << endl;
    my_buffer.clear();
    size_t len = my_recv(server_fd, my_buffer);
    if (len < 0) {
        cerr << "loop recv failure" << endl;
        return;
    } else if (len == 0) {
        cerr << "close tunnel" << endl;
        return;
    }
    //  cout << proxy_id << ": Received Successfully in re_fetching" << endl;
    string str = "";
    int i = 0;
    while(i < my_buffer.size()){
        str += my_buffer[i].data();
        i++;
    }
    response_refetched = new Response(str);
}

void Proxy::post_request() {
    re_fetching();
    if(my_send(client_fd, my_buffer) < 0){
        cerr << "Error: reply to client" << endl;
    }
    cout << "=========" << proxy_id << ": Successdully Reply ========" << endl;
}

bool Proxy::revalidation(){
    
    // construct revalidation request
    int status;
    Response *cached_resp = cache.get(request->get_full_url());
    if (cached_resp == nullptr) {
        cout << proxy_id << ": Error: Cache Not Found" << endl;
        return false;
    }
    cerr << proxy_id << ": wtf111" << endl;
    std::map<std::string, std::string> *cached_resp_header = cached_resp->get_header();
    std::string newheader = "";
    auto it_etag = cached_resp_header->find("Etag");
    if (it_etag != cached_resp_header->end()) {
        cout << proxy_id << ": " << "Etag: " <<it_etag->second << endl;
        newheader += string("\r\n") + string("If-None-Match:") + it_etag->second;
    }
    else {
        auto it_mod = cached_resp_header->find("Last-Modified");
        if (it_mod != cached_resp_header->end()) {
            cout << proxy_id << ": " << "Last-Modified: " <<it_mod->second << endl;
            newheader += string("\r\n")+ string("If-Modified-Since:") + it_mod->second;
        }
        else {
            cerr << proxy_id << ": not found etag or last modified" << endl;
            return false;
        }
    }
    cout << proxy_id << ": wtf222" << endl;

    // string req = request->get_request();
    // string req_content = req.substr(0, req.find("\r\n\r\n")+4);
    // req_content.insert(req_content.find("\r\n\r\n"), newheader);
    // cout << req_content << endl;

    string req = request->get_request();
    cout<< "##############################"<<proxy_id << req<<"##############################"<<endl;
    // string req_content = req.insert(req.find("\r\n\r\n"), newheader);
    vector<vector<char>> tempbuffer;
    // resp_to_buf(tempbuffer, req_content);
    resp_to_buf(tempbuffer, req);
    for(int i=0;i<tempbuffer.size();++i){
        std::cout << "***************************************************"<<tempbuffer[i].data() << endl;
    }
    status = my_send(server_fd, my_buffer);

    //send revalidation
    // vector<char> temp(req_content.begin(), req_content.end());
    // status = send(server_fd, temp.data(), temp.size(), 0);
    if (status < 0) {
        cout<<proxy_id <<": revalidation send failure" <<endl;
        return false;
    }


//    std::vector<char> buffer(9999);
//    char* p = buffer.data();
//    status = recv(server_fd, p, 9999, 0);
    vector<char> recvtemp(9999);
    status = recv(server_fd, recvtemp.data(), recvtemp.size(), 0);

    if (status < 0) {
        cout<<proxy_id <<": revalidation recv failure" <<endl;
        return false;
    }

    char* p = recvtemp.data();

    std::string s(p);

    // identify code
    string code = s.substr(s.find(" ") + 1, 3);
    cout<<proxy_id <<":"<< code <<endl;
    if (code == "200") {
        return true;
    }
    if (code != "304" && code != "200") {
        cout<<proxy_id<<": not 200 304"<<endl;
        return false;
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
            cerr << proxy_id << ": Cannot Cache, no-store/private/Authorization" << endl;
            return false;
        }
        // if(cache_control.find("max-age")!=string::npos) {
        //     if (date == header->end() && age == header->end() && exp == header->end()) {
        //         cerr << proxy_id << ": Cannot Cache, only max-age" << endl;
        //         return false;
        //     }
        // } else {
        //     if (exp == header->end()){
        //         cerr << proxy_id << ": Cannot Cache, no max-age and expire" << endl;
        //         return false;
        //     }
        // }
    }
    // else{
    //     if(exp == header->end()){
    //         cerr << proxy_id << ": Cannot Cache, no cache control and expire" << endl;
    //         return false;
    //     }
    // }
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

bool Proxy::can_get_expinfo(){
    std::map<std::string, std::string> *header = response_back->get_header();
    auto cc = header->find("Cache-Control");
    auto exp = header->find("Expires");
    auto date = header->find("Date");
    auto age = header->find("Age");
    std::string cache_control;

    if (cc != header->end()){
        cache_control = cc->second;
        if(cache_control.find("max-age")!=string::npos) {
            if (date == header->end() && age == header->end() && exp == header->end()) {
                cerr << proxy_id << ": only max-age, no exp or date or age" << endl;
                return false;
            }
        } else {
            if (exp == header->end()){
                cerr << proxy_id << ": no max-age and expire" << endl;
                return false;
            }
        }
    }else{
        if(exp == header->end()){
            cerr << proxy_id << ": no cache control and expire" << endl;
            return false;
        }
    }
    return true;
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
        // if()
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

size_t Proxy::my_recv(int fd, vector<vector<char>> &mybuffer){
    vector<char> first_buf(65535);
    char *p = first_buf.data();
    size_t len = 0;
    if((len = recv(fd, p, 65535, 0))<=0){
        cerr << proxy_id << ": first recv " << len << " failure" << endl;
        return len;
    }
    mybuffer.push_back(first_buf);
    // mybuffer is the buffer taking the whole response

    string first_str(first_buf.begin(), first_buf.end());
    // get header
    string header_str = first_str.substr(0, first_str.find("\r\n\r\n") + 4);
    auto pos_chk = header_str.find("chunked");
    auto pos_clen = header_str.find("Content-Length: ");
    if(pos_chk != string::npos) {
        // chunked
        cout << "chunked" << endl;
        while(true) {
            size_t once_len;
            if((once_len = continue_recv(fd)) <= 0) {
                return len;
            }
            len += once_len;
            vector<char> last_buf = mybuffer[mybuffer.size()-1];
            if (strstr(last_buf.data(), "0\r\n\r\n") != nullptr) {
                break;
            }
        }
        return len;
    }
    else if(pos_clen != string::npos) {

        string content = header_str.substr(header_str.find("Content-Length: "));
        content = content.substr(16, content.find("\r\n"));
        size_t total = (size_t)atoi(content.c_str());

        // if cur_len < total_content_length + header_size
        while (len < total + header_str.size()){
            size_t once_len = 0;
            if((once_len = continue_recv(fd)) <= 0){
                cerr << "continue recv failure" << endl;
                return len;
            }
            len += once_len;
        }
        cout << "Content-Length: " << total << endl;
        return len;
    }
    return len;
}

size_t Proxy::continue_recv(int fd) {
    vector<char> continue_buf(65535);
    char *p = continue_buf.data();
    size_t once_len = 0;
    if((once_len = recv(fd, p, 65535, 0)) <= 0){
        return once_len;
    }
    my_buffer.push_back(continue_buf);
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
            cerr << proxy_id << ": my_send Failed" << endl;
            return once_len;
        }
        len += once_len;
        sent += 1;
    }
    cout << proxy_id << ": sent " << len << " bytes" << endl;
    return len;
}


void Proxy::resp_to_buf(vector<vector<char>> &mybuffer, string response){
    size_t temp_size = 0;
    size_t full_size = buffer_size;
    size_t remain_size = response.size();
    string content = "";
    while(remain_size > 0) {
        if(remain_size < full_size) {
            content = response.substr(temp_size, remain_size);
            vector<char> temp(content.begin() , content.end());
            mybuffer.push_back(temp);
            break;
        }
        content = response.substr(temp_size, full_size);
        vector<char> temp(content.begin() , content.end());
        mybuffer.push_back(temp);
        temp_size = temp_size + full_size;
        remain_size = remain_size - full_size;
    }
}

void Proxy::send_cached_response(){
    string resp_str = response_back->get_response();
    my_buffer.clear();
    resp_to_buf(my_buffer, resp_str);
    size_t len = my_send(client_fd, my_buffer);
    if (len <= 0) {
        cerr << proxy_id << ": Reply to Client Failure" << endl;
        return;
    }
    cerr << proxy_id << ": Reply to Client Success!!!" << endl;
    return;
}
#endif //PROXY_PROXY_H
