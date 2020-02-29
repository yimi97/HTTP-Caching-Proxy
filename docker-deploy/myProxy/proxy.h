//
// Created by Ying Xu & Yi Mi
//

#ifndef PROXY_PROXY_H
#define PROXY_PROXY_H
#include <ctime>
#include <iostream>
#include <locale>
#include <iomanip>
#include <sstream>
#include <time.h>
#include <netdb.h>
#include <typeinfo>
#include <sstream>
#include "request.h"
#include "cache.h"
#include <fstream>
#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MYLOG "/var/log/erss/proxy.log"
const int buffer_size = 65535;

class Proxy {
private:
    Request* request;
    std::map<std::string, std::string> *req_header;

    int proxy_id;
    int client_fd;
    string client_ip;
    int server_fd;
    int content_length;
    ofstream log_flow;

    Response* response_back; // cached
    Response* response_refetched;
    std::map<std::string, std::string> *resp_header;
    vector<vector<char>> my_buffer;
    void get_request_from_client();
    void send_cached_response();
    void connect_request(); // if request.method == "CONNECT"
    bool connect_with_server();
    void send_ack();
    void data_forwarding();
    void re_fetching();
    void get_request(); // if request.method == "GET"
    bool can_get_expinfo(Response* res);
    time_t get_time(string date_or_exp);
    time_t get_expiration_time(Response* res);
    bool revalidation();
    bool can_update();
    void update_cache();
    void log(const char* str);
    size_t my_recv(int fd, vector<vector<char>> &mybuffer);
    size_t my_send(int fd, vector<vector<char>> &buf);
    size_t continue_recv(int fd, vector<vector<char>>& buf);
    void resp_to_buf(vector<vector<char>> &mybuffer, string response);
    void post_request(); // if request.method == "POST"
    void send_400();
    void send_502();
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
        delete request;
    }

    void proxy_run() {
        get_request_from_client();
        if (request == nullptr) {
            cout << proxy_id << ": Did not receive any request ENC================" << endl;
            close(client_fd);
            return;
        } else {
            log_flow.open(MYLOG,std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": " << request->get_request_line() << " from " << client_ip << " @ " << request->get_recv_time() << endl;
            log_flow.close();
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
    while(true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(client_fd, &readfds);
        int status = select(fdmax + 1, &readfds, nullptr, nullptr, nullptr);
        if (status <= 0) {
            log("ERROR Select Failure");
            break;
        }
        if (FD_ISSET(server_fd, &readfds)) {
            // recv data from server
            status = recv(server_fd, p, buffer_size, 0);
            if (status < 0) {
                send_502();
                log("ERROR 502 Bad Gateway: Receive Data From Server Failure");
                break;
            } else if (status == 0) {
                log("Tunnel closed");
                return;
            } else {
                // forward data to client
                
                /* 
                 * check if server responds 400 Bad Request 
                **/
                string s(buffer.data());
                string code400 = s.substr(s.find(" ") + 1, 3);
                if (code400 == "400") {
                    send_400();
                    log("ERROR 400 Bad Request");
                    break;
                }

                status = send(client_fd, p, status, 0);
                if (status == -1) {
                    log("ERROR Transfer Data to Client Failure");
                    break;
                }
            }
        } else if (FD_ISSET(client_fd, &readfds)) {
            // recv data from client
            status = recv(client_fd, p, buffer_size, 0);
            if (status < 0) {
                log("ERROR Receive Data From Client Failure");
                break;
            } else if (status == 0) {
                log("Tunnel closed");
                return;
            } else {
                // forward data to server
                status = send(server_fd, p, status, 0);
                if (status == -1) {
                    log("ERROR Transfer Data to Client Failure");
                    break;
                }
            }
        }
    } // while
}

void Proxy::get_request_from_client() {
    // get request from client
    my_buffer.clear();
    //======================
//    vector<char> buffer(buffer_size);
//    char* p = buffer.data();
//    int len = recv(client_fd, p, buffer_size, 0);
            //==================
    int len = my_recv(client_fd, my_buffer);
    if (len < 0) {
        log("ERROR Receive Request from Client Failure");
        return;
    }
    if (len == 0) {
        log("NOTE Tunnel Closed");
        return;
    }
    string str = "";
    //===================
    int i = 0;
    while(i < my_buffer.size()){
        str += my_buffer[i].data();
        i++;
    }
    //====================
    /*
     * Get client's ip address which will be printed in log
     * Last modified by Ying
     * ref: https://beej.us/guide/bgnet/html//index.html#connect (getpeername())
     * */
    struct sockaddr_storage addr;
    socklen_t socklen = sizeof(addr);
    char ipstr[INET6_ADDRSTRLEN];
    int port;
    getpeername(client_fd, (struct sockaddr*)&addr, &socklen);

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in *)&addr;
        port = ntohs(p->sin_port);
        inet_ntop(AF_INET, &p->sin_addr, ipstr, sizeof(ipstr));
    } else { // AF_INET6
        struct sockaddr_in6 *p = (struct sockaddr_in6 *)&addr;
        port = ntohs(p->sin6_port);
        inet_ntop(AF_INET6, &p->sin6_addr, ipstr, sizeof(ipstr));
    }
    client_ip = ipstr;

    request = new Request(str, proxy_id);
    req_header = request->get_header();
    cout << endl << "================= " << proxy_id << ": " << request->get_method() << " Request Received =================" << endl;
    cout << proxy_id << ": " << request->get_request_line() << endl;
    cout << proxy_id << ": " << request->get_port() << endl;
    cout << proxy_id << ": " << request->get_host() << endl;
}

void Proxy::connect_request() {
    // connect with server
    try{
        if (!connect_with_server()) {
            log("ERROR Connect to Server Failure");
            return;
        }
    } catch (exception &e) {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": Connect with the Server Failure: " << e.what() << endl;
        log_flow.close();
        return;
    }
    // send 200 OK back to client
    send_ack();
    // data transfer
    try{
        data_forwarding();
    } catch (std::exception &e) {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": Data Forwarding Failure: " << e.what() << endl;
        log_flow.close();
        return;
    }
    return;
}

bool Proxy::connect_with_server() {
    struct addrinfo host_info;
    struct addrinfo *p = &host_info;
    struct addrinfo *host_info_list;
    int status;
    memset(&host_info, 0, sizeof(host_info));//make sure the struct is empty
    host_info.ai_family   = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(request->get_host().c_str(),
            request->get_port().c_str(),
            &host_info,
            &host_info_list);
    if (status != 0) {
        log("ERROR cannot get address info for host");
        return false;
    }
    server_fd = socket(host_info_list->ai_family,
            host_info_list->ai_socktype,
            host_info_list->ai_protocol);
    if (server_fd == -1) {
        log("ERROR cannot create socket");
        return false;
    }
    /* connect */
    status = connect(server_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        log("ERROR cannot connect to socket");
        return false;
    }
    log("NOTE Connect server successfully");

    freeaddrinfo(host_info_list);
    return true;
}

void Proxy::get_request(){
    string url = request->get_full_url();
    Response *cached_resp = cache.get(url);
    response_back = cached_resp;
    try{
        if (!connect_with_server()) {
            log("ERROR Connect to Server Failure");
            return;
        }
    } catch (exception &e) {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": Connect with the Server Failure: " << e.what() << endl;
        log_flow.close();
        return;
    }
    /*
         * Mi Yi taking care of this part
         * 
         * if no expirtion info, revalidation
         *
         * if expired, refetching
         * if not expired, check cache-control
         *
         * request:
         * if cache-control -> no cache -> revalidation
         * if cache-control -> no store -> re-fetching
         * response:
         * if cache-control -> no cache/must-revalidate/proxy-revalidate/Authorization -> revalidation
         * if valid and not expired, send back to client.
         *
         */
    if (cached_resp != nullptr) {
        if(!can_get_expinfo(cached_resp)){
            log("in cache, no expire info, requires validation");
            bool valid = revalidation();
            if (valid) {
                log("NOTE Re-validation Succeed ");
                // =======================================
                send_cached_response();
                // TBD ===================================
                return;
            }
            else {
                log("NOTE Re-validation Failed, Re-fetching ");
                // refetching
                re_fetching();
                // update
                update_cache();
                int status = my_send(client_fd, my_buffer);
                if (status < 0) {
                    log("ERROR Reply to Client Failure");
                    return;
                }
                if (status == 0) {
                    log("NOTE Tunnel Closed");
                    return;
                }
                log("NOTE Reply to Client Successfully");
                return;
            }
        }
        // check if expire
        time_t expired_time = get_expiration_time(cached_resp);
        time_t now = time(0);
        std::cout << "In cache, expiration time " << asctime(gmtime(&expired_time)) << std::endl;
        if (difftime(now, expired_time)>0) {
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": in cache, but expired at " << asctime(gmtime(&expired_time)) << endl;
            log_flow.close();
            // refetching
            re_fetching();
            // update cache
            update_cache();
            int status = my_send(client_fd, my_buffer);
            if( status < 0) {
                log("ERROR Reply to Client Failure");
                return;
            }
            if( status == 0) {
                log("NOTE Tunnel Closed");
                return;
            }

            log("NOTE Reply to Client Successfully");
            return;
        }
        else {// if not expired
            // request cache-control
            auto req_it = req_header->find("Cache-Control");
            if (req_it != req_header->end()) {
                // Found Request Cache-Control
                string request_cache_control = req_it->second;
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": NOTE Cache-Control: " << request_cache_control << endl;
                log_flow.close();
                // no-cahce -> revalidate
                if (request_cache_control.find("no-cache") != string::npos) {
                    // revalidate
                    log("in cache, requires re-validation");
                    bool valid = revalidation();
                    if (valid) {
                        log("NOTE Re-validation Successeed ");
                        send_cached_response();
                        return;
                    }
                    else {
                        log("NOTE Re-validation Failed, Re-Fetching");
                        // refetching
                        re_fetching();
                        // update
                        update_cache();
                        int status = my_send(client_fd, my_buffer);
                        if( status < 0) {
                            log("ERROR Reply to Client Failure");
                            return;
                        }
                        if( status == 0) {
                            log("NOTE Tunnel Closed");
                            return;
                        }
                        log("NOTE Reply to Client Successfully");
                        return;

                    }

                }
                // no-store -> refetching
                if (req_header->find("no-store")!=req_header->end()){
                    log("in cache, requires re-fetching");
                    // refetching
                    re_fetching();
                    // update
                    update_cache();
                    int status = my_send(client_fd, my_buffer);
                    if( status < 0) {
                        log("ERROR Reply to Client Failure");
                        return;
                    }
                    if( status == 0) {
                        log("NOTE Tunnel Closed");
                        return;
                    }
                    log("NOTE Reply to Client Successfully");
                    return;
                }
            }
            // response cache-control
            std::map<string, string> *cached_resp_header = cached_resp->get_header();
            auto resp_it = cached_resp_header->find("Cache-Control");
            if (resp_it != cached_resp_header->end()) {
                string response_cached_cache_control = resp_it->second;
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": NOTE Cache-Control: " << response_cached_cache_control << endl;
                log_flow.close();
                if (response_cached_cache_control.find("no-cache") != string::npos ||
                    response_cached_cache_control.find("must-revalidate") != string::npos ||
                    response_cached_cache_control.find("proxy-revalidate") != string::npos ||
                    response_cached_cache_control.find("Authorization") != string::npos) {
                    log("in cache, requires re-validation");
                    bool valid = revalidation();
                    if (valid) {
                        log("NOTE Re-validation Successeed");
                        send_cached_response();
                        return;
                    }
                    else {
                        log("NOTE Re-validation Failed, Re-fetching");
                        // refetching
                        re_fetching();
                        // update
                        update_cache();
                        // reply to client
                        int status = my_send(client_fd, my_buffer);
                        if( status < 0) {
                            log("ERROR Reply to Client Failure");
                            return;
                        }
                        if( status == 0) {
                            log("NOTE Tunnel Closed");
                            return;
                        }
                        log("NOTE Reply to Client Successfully");
                        return;
                    }
                }
            }
            log("in cache, valid");
            send_cached_response();
        }
    } else {
        // Not Found + Re-fetching
        log("not in cache");
        re_fetching();
        // Update Cache
        update_cache();
        // Reply to Client
        int status = my_send(client_fd, my_buffer);
        if( status < 0) {
            log("ERROR Reply to Client Failure");
            return;
        }
        if( status == 0) {
            log("NOTE Tunnel Closed");
            return;
        }
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": Responding \"" << response_refetched->get_status_line() << "\"" << endl;
        log_flow.close();
    }
}

void Proxy::re_fetching(){
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": Requesting \"" << request->get_request_line() << "\" from " << request->get_host() << endl;
    log_flow.close();
//    int status = my_send(server_fd, my_buffer);
    int status = my_send(server_fd, my_buffer);
    if( status < 0) {
        log("ERROR Reply to Server Failure");
        return;
    }
    if( status == 0) {
        log("NOTE Tunnel Closed");
        return;
    }
    log("NOTE Send Request to Server Successfully");
    my_buffer.clear();
    size_t len = my_recv(server_fd, my_buffer);
    if (len < 0) {
        send_502();
        log("ERROR 502 Bad Gateway: Receive Response from Server Failure");
        return;
    }
    if (len == 0) {
        log("NOTE Tunnel Closed");
        return;
    }
    /* 
        * check if server responds 400 Bad Request 
    **/
    string s(my_buffer[0].data());
    string code400 = s.substr(s.find(" ") + 1, 3);
    if (code400 == "400") {
        send_400();
        log("ERROR 400 Bad Request");
        return;
    }

    log("NOTE Receive Response from Server Successfully");
    string str = "";
    int i = 0;
    while(i < my_buffer.size()){
        str += my_buffer[i].data();
        i++;
    }
    response_refetched = new Response(str);
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": Received \"" << response_refetched->get_status_line() << "\" from " << request->get_host() << endl;
    log_flow.close();
    return;
}

void Proxy::post_request() {
    re_fetching();
    int status = my_send(client_fd, my_buffer);
    if( status < 0) {
        log("ERROR Reply to Client Failure");
        return;
    }
    if( status == 0) {
        log("NOTE Tunnel Closed");
        return;
    }
    log("NOTE Reply to Client Successfully");
    return;
}

bool Proxy::revalidation(){
    // construct revalidation request
    int status;
    Response *cached_resp = cache.get(request->get_full_url());
    if (cached_resp == nullptr) {
        log("ERROR Cached-Response Not Found");
        return false;
    }
    std::map<std
    ::string, std::string> *cached_resp_header = cached_resp->get_header();
    std::string newheader = "";
    auto it_etag = cached_resp_header->find("ETag");
    if (it_etag != cached_resp_header->end()) {
        string etag_str = it_etag->second;
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": NOTE ETag: " << etag_str << endl;
        log_flow.close();
        newheader += string("\r\n") + string("If-None-Match:") + etag_str;
    }
    else {
        auto it_mod = cached_resp_header->find("Last-Modified");
        if (it_mod != cached_resp_header->end()) {
            string lst_mdf_str = it_mod->second;
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": NOTE Last-Modified: " << lst_mdf_str << endl;
            log_flow.close();
            newheader += string("\r\n")+ string("If-Modified-Since:") + lst_mdf_str;
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
    if (status <= 0) {
        log("ERROR Send Re-validation Request Failure");
        return false;
    }
    std::vector<char> buffer(9999);
    char* p = buffer.data();
    status = recv(server_fd, p, 9999, 0);
    if (status <= 0) {
        send_502();
        log("ERROR 502 Bad Gateway: Receive Re-validation Response Failure");
        return false;
    }
    std::string s(p);
    // identify code
    string code = s.substr(s.find(" ") + 1, 3);
    if (code == "304") {
        log("NOTE Received 304 \"NOT modified\" from Server");
        return true;
    }
    log(": NOTE Received NOT 304 \"Modified\" from Server");
    return false;
}

bool Proxy::can_update() {
    cout << "can_update" << endl;
    std::map<std::string, std::string> *header = response_refetched->get_header();
    auto cc = header->find("Cache-Control");
    auto exp = header->find("Expires");
    auto date = header->find("Date");
    auto age = header->find("Age");
    std::string cache_control;
    if (cc != header->end()) {
        cache_control = cc->second;
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": NOTE Cache-Control: " << cache_control << endl;
        log_flow.close();
        if (cache_control.find("no-store") != string::npos) {
            log("not cacheable because no-store");
            return false;
        } else if (cache_control.find("private") != string::npos) {
            log("not cacheable because private");
            return false;
        }
    }
    return true;
}

void Proxy::update_cache(){
    if (can_update()) {
        cache.put(request->get_full_url(), response_refetched);
        log("NOTE Update Cache Successfully");
        if (can_get_expinfo(response_refetched)) {
            time_t refetched_exptime = get_expiration_time(response_refetched);
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": cached, expires at " << asctime(gmtime(&refetched_exptime)) << endl;
            log_flow.close();
        } else {
            log("cached, but requires re-validation");
        }
    } else {
        delete response_refetched;
    }
}

time_t Proxy::get_time(string date_or_exp) {
    int n = date_or_exp.length();
    char char_array[n + 1];
    strcpy(char_array, date_or_exp.c_str());
    const char *time_details = char_array;
    struct tm tm;
//    memset(&tm, 0, sizeof(tm));
    strptime(time_details, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    time_t t = mktime(&tm);
    return t;
}

time_t Proxy::get_expiration_time(Response* res){
    std::map<std::string, std::string> *header = res->get_header();
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

size_t Proxy::my_recv(int fd, vector<vector<char>> &mybuffer){
    vector<char> first_buf(buffer_size);
    char *p = first_buf.data();
    size_t len = 0;
    if((len = recv(fd, p, 65535, 0)) <= 0){
        log("ERROR my_recv() First Receive Failure");
        return len;
    }
    first_buf.resize(len);
    mybuffer.push_back(first_buf);
    // mybuffer is the buffer taking the whole response
    string first_str(first_buf.begin(), first_buf.end());
    // get header
    string header_str = first_str.substr(first_str.find("\r\n") + 2);
    header_str = header_str.substr(0, first_str.find("\r\n\r\n") + 4);
    cout << proxy_id << ": " << header_str << endl;
    auto pos_chk = header_str.find("chunked");
    auto pos_clen = header_str.find("Content-Length: ");
    if(pos_chk != string::npos) {
        // chunked
        log("NOTE chunked");
        cout << "chunked" << endl;
        int i = 0;
        while(true) {
            size_t once_len;
            if((once_len = continue_recv(fd, my_buffer)) < 0) {
                log("ERROR my_recv() chunked Loop Receive Failure");
                return len;
            } else if (once_len == 0) {
                log("NOTE finished recv");
                break;
            }
//            cout << proxy_id << ": once: " << i << endl;
            len += once_len;
            vector<char> last_buf = mybuffer[mybuffer.size()-1];
            if (strstr(last_buf.data(), "0\r\n\r\n") != nullptr) {
                log("NOTE last chunk arrived");
                break;
            }
            i++;
        }
        return len;
    }
    else if(pos_clen != string::npos) {
        string content = header_str.substr(header_str.find("Content-Length: "));
        content = content.substr(16, content.find("\r\n"));
        size_t total = (size_t)atoi(content.c_str());
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": NOTE Content-Length: " << total << endl;
        log_flow.close();
        int i = 0;
        while (len < total + header_str.size()){
            size_t once_len = 0;
            if((once_len = continue_recv(fd, mybuffer)) < 0){
                log("ERROR my_recv() no c/ch Loop Receive Failure");
                return len;
            } else if (once_len == 0) {
                log("NOTE finished recv");
                break;
            }
            len += once_len;
        }
        return len;
    } else {
        return len;
    }
    return len;
}

size_t Proxy::continue_recv(int fd, vector<vector<char>>& buf) {
    vector<char> continue_buf(buffer_size);
    char *p = continue_buf.data();
    size_t once_len = 0;

    try {
        cout << proxy_id << ": in continue recv: " << once_len << endl;
        if((once_len = recv(fd, p, buffer_size, 0)) <= 0) {
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": WARNING continue recv() <= 0" << endl;
            log_flow.close();
            return once_len;
        }
        cout << proxy_id << ": check once: " << once_len << endl;

    } catch (exception &e) {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": ERROR Bad continue_recv: " << e.what() << endl;
        log_flow.close();
    }

    continue_buf.resize(once_len);
    cout << proxy_id << ": actually continue recv: " << once_len << endl;
    cout << proxy_id << ": good continue recv: " << continue_buf.capacity() << endl;
    buf.push_back(continue_buf);
    return once_len;
}

size_t Proxy::my_send(int fd, vector<vector<char>>& buf) {
    size_t len = 0;
    int sent = 0;
    try{
//        cout << proxy_id << ": SHOULD send: " << buf.size() << endl;
        while(sent < buf.size()) {
//            cout << proxy_id << ": check send once: " << sent << " cur_size: " << buf[sent].capacity() << endl;
            int once_len = send(fd, buf[sent].data(), buf[sent].capacity(), 0);
            if (once_len <= 0){
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": WARNING send() <= 0" << endl;
                log_flow.close();
                return once_len;
            }
//            cout << proxy_id << ": good send once: " << sent << endl;
            len += once_len;
            sent += 1;
        }
    } catch (exception &e) {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": my_send Failure: " << e.what() << endl;
        log_flow.close();
    }
    return len;
}

bool Proxy::can_get_expinfo(Response* res){
    std::map<std::string, std::string> *header = res->get_header();
    auto cc = header->find("Cache-Control");
    auto exp = header->find("Expires");
    auto date = header->find("Date");
    auto age = header->find("Age");
    std::string cache_control;

    if (cc != header->end()){
        cache_control = cc->second;
        if(cache_control.find("max-age")!=string::npos) {
            if (date == header->end() && age == header->end() && exp == header->end()) {
                log("NOTE only max-age, no exp or date or age");
                return false;
            }
        } else {
            if (exp == header->end()){
                log("NOTE no max-age and expire");
                return false;
            }
        }
    }else{
        if(exp == header->end()){
            log("NOTE no cache control and expire");
            return false;
        }
    }
    return true;
}

void Proxy::resp_to_buf(vector<vector<char>> &mybuffer, string response){
    size_t temp_size = 0;
    size_t full_size = buffer_size;
    size_t remain_size = response.size();
    string content;
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
    if (len < 0) {
        log("NOTE Send Cached-Response Back Failure");
        return;
    }
    if( len == 0) {
        log("NOTE Tunnel Closed");
        return;
    }

    log("NOTE Send Cached-Response Back Successfully");
    return;
}

void Proxy::log(const char* str){
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": " << str << endl;
    log_flow.close();
}

void Proxy::send_400(){
    string header_400 = "HTTP/1.1 400 Bad Request\r\n\r\n";
    int status = send(client_fd, header_400.c_str(), header_400.size(), 0);
    if (status <= 0) {
        log("ERROR 400 but sending to cliend failed");
        return;
    }
}

void Proxy::send_502(){
    string header_502 = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    int status = send(client_fd, header_502.c_str(), header_502.size(), 0);
    if (status <= 0) {
        log("ERROR 502 but sending to cliend failed");
        return;
    }
}


#endif //PROXY_PROXY_H
