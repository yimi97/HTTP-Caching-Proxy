//
// Created by 徐颖 on 2/17/20.
//

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
    size_t my_recv(int fd, vector<vector<char>> &mybuffer);
    size_t my_send(int fd, vector<vector<char>> &buf);
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
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": ERROR Select Failure" << endl;
            log_flow.close();
            break;
        }
        if (FD_ISSET(server_fd, &readfds)) {
            // recv data from server
            status = recv(server_fd, p, buffer_size, 0);
            if (status < 0) {
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": ERROR Receive Data From Server Failure" << endl;
                log_flow.close();
                break;
            } else if (status == 0) {
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": Tunnel closed" << endl;
                log_flow.close();
                return;
            } else {
                // forward data to client
                status = send(client_fd, p, status, 0);
                if (status == -1) {
                    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                    log_flow << proxy_id << ": ERROR Transfer Data to Client Failure" << endl;
                    log_flow.close();
                    break;
                }
            }
        } else if (FD_ISSET(client_fd, &readfds)) {
            // recv data from client
            status = recv(client_fd, p, buffer_size, 0);
            if (status < 0) {
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": ERROR Receive Data From Client Failure" << endl;
                log_flow.close();
                break;
            } else if (status == 0) {
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": Tunnel closed" << endl;
                log_flow.close();
                return;
            } else {
                // forward data to server
                status = send(server_fd, p, status, 0);
                if (status == -1) {
                    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                    log_flow << proxy_id << ": ERROR Transfer Data to Server Failure" << endl;
                    log_flow.close();
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
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": ERROR Receive Request from Client Failure" << endl;
        log_flow.close();
        return;
    }
    string str = "";
    int i = 0;
    while(i < my_buffer.size()){
        str += my_buffer[i].data();
        i++;
    }
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
    /*
         * Mi Yi taking care of this part
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
        if(!can_get_expinfo(cached_resp)){
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": in cache, no expire info, requires validation" << endl;
            log_flow.close();
            bool valid = revalidation();
            if (valid) {
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": NOTE Re-validation Succeed " << endl;
                log_flow.close();
                // =======================================
                send_cached_response();
                // TBD ===================================
                return;
            }
            else {
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": NOTE Re-validation Failed, Re-fetching " << endl;
                log_flow.close();
                // refetching
                re_fetching();
                // update
                if (can_update()) {
                    cache.put(request->get_full_url(), response_refetched);
                    if (can_get_expinfo(response_refetched)) {
                        time_t refetched_exptime = get_expiration_time(response_refetched);
                        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                        log_flow << proxy_id << ": cached, expires at " << asctime(gmtime(&refetched_exptime)) << endl;
                        log_flow.close();
                    } else {
                        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                        log_flow << proxy_id << ": cached, but requires re-validation" << endl;
                        log_flow.close();
                    }
                }
                if(my_send(client_fd, my_buffer) <= 0){
                    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                    log_flow << proxy_id << ": ERROR Reply to Client Failure" << endl;
                    log_flow.close();
                    return;
                }
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": NOTE Reply to Client Successfully" << endl;
                log_flow.close();
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
            if (can_update()) {
                cache.put(request->get_full_url(), response_refetched);
                if (can_get_expinfo(response_refetched)) {
                    time_t refetched_exptime = get_expiration_time(response_refetched);
                    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                    log_flow << proxy_id << ": cached, expires at " << asctime(gmtime(&refetched_exptime)) << endl;
                    log_flow.close();
                } else {
                    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                    log_flow << proxy_id << ": cached, but requires re-validation" << endl;
                    log_flow.close();
                }
            }
            if(my_send(client_fd, my_buffer) <= 0){
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": ERROR Reply to Client Failure" << endl;
                log_flow.close();
                return;
            }
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": NOTE Reply to Client Successfully" << endl;
            log_flow.close();
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
                    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                    log_flow << proxy_id << ": in cache, requires re-validation" << endl;
                    log_flow.close();
                    bool valid = revalidation();
                    if (valid) {
                        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                        log_flow << proxy_id << ": NOTE Re-validation Successeed " << endl;
                        log_flow.close();
                        // =======================================
                        send_cached_response();
                        // TBD ===================================
                        return;
                    }
                    else {
                        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                        log_flow << proxy_id << ": NOTE Re-validation Failed, Re-Fetching" << endl;
                        log_flow.close();
                        // refetching
                        re_fetching();
                        // update
                        if (can_update()) {
                            cache.put(request->get_full_url(), response_refetched);
                            if (can_get_expinfo(response_refetched)) {
                                time_t refetched_exptime = get_expiration_time(response_refetched);
                                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                                log_flow << proxy_id << ": cached, expires at " << asctime(gmtime(&refetched_exptime)) << endl;
                                log_flow.close();
                            } else {
                                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                                log_flow << proxy_id << ": cached, but requires re-validation" << endl;
                                log_flow.close();
                            }
                        }
                        if(my_send(client_fd, my_buffer) <= 0){
                            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                            log_flow << proxy_id << ": ERROR Reply to Client Failure" << endl;
                            log_flow.close();
                            return;
                        }
                        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                        log_flow << proxy_id << ": NOTE Reply to Client Successfully" << endl;
                        log_flow.close();
                        return;
                    }

                }
                // no-store -> refetching
                if (req_header->find("no-store")!=req_header->end()){
                    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                    log_flow << proxy_id << ": in cache, requires re-fetching" << endl;
                    log_flow.close();
                    // refetching
                    re_fetching();
                    // update
                    if (can_update()) {
                        cache.put(request->get_full_url(), response_refetched);
                        if (can_get_expinfo(response_refetched)) {
                            time_t refetched_exptime = get_expiration_time(response_refetched);
                            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                            log_flow << proxy_id << ": cached, expires at " << asctime(gmtime(&refetched_exptime)) << endl;
                            log_flow.close();
                        } else {
                            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                            log_flow << proxy_id << ": cached, but requires re-validation" << endl;
                            log_flow.close();
                        }
                    }
                    if(my_send(client_fd, my_buffer) <= 0){
                        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                        log_flow << proxy_id << ": ERROR Reply to Client Failure" << endl;
                        log_flow.close();
                        return;
                    }
                    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                    log_flow << proxy_id << ": NOTE Reply to Client Successfully" << endl;
                    log_flow.close();
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
                    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                    log_flow << proxy_id << ": in cache, requires re-validation" << endl;
                    log_flow.close();
                    bool valid = revalidation();
                    if (valid) {
                        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                        log_flow << proxy_id << ": NOTE Re-validation Successeed" << endl;
                        log_flow.close();
                        // =============================================
                        send_cached_response();
                        // ==============================================
                        return;
                    }
                    else {
                        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                        log_flow << proxy_id << ": NOTE Re-validation Failed, Re-fetching" << endl;
                        log_flow.close();
                        // refetching
                        re_fetching();
                        // update
                        if (can_update()) {
                            cache.put(request->get_full_url(), response_refetched);
                            if (can_get_expinfo(response_refetched)) {
                                time_t refetched_exptime = get_expiration_time(response_refetched);
                                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                                log_flow << proxy_id << ": cached, expires at " << asctime(gmtime(&refetched_exptime)) << endl;
                                log_flow.close();
                            } else {
                                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                                log_flow << proxy_id << ": cached, but requires re-validation" << endl;
                                log_flow.close();
                            }
                        }
                        // reply to client
                        if(my_send(client_fd, my_buffer) <= 0){
                            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                            log_flow << proxy_id << ": ERROR Reply to Client Failure" << endl;
                            log_flow.close();
                            return;
                        }
                        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                        log_flow << proxy_id << ": NOTE Reply to Client Successfully" << endl;
                        log_flow.close();
                        return;
                    }
                }
            }
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": in cache, valid" << endl;
            log_flow.close();
            send_cached_response();
        }
    } else {
        // Not Found + Re-fetching
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": not in cache" << endl;
        log_flow.close();
        re_fetching();
        // Update Cache
        if (can_update()) {
            cache.put(request->get_full_url(), response_refetched);
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": NOTE, Update Cache Successfully" << endl;
            log_flow.close();
            if (can_get_expinfo(response_refetched)) {
                time_t refetched_exptime = get_expiration_time(response_refetched);
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": cached, expires at " << asctime(gmtime(&refetched_exptime)) << endl;
                log_flow.close();
            } else {
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": cached, but requires re-validation" << endl;
                log_flow.close();
            }
        }
        // Reply to Client
        if(my_send(client_fd, my_buffer) <= 0){
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": ERROR Reply to Client Failure" << endl;
            log_flow.close();
            return;
        }
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": Responding \"" << response_refetched->get_status_line() << "\"" << endl;
        log_flow.close();
    }
}

void Proxy::re_fetching(){
    if (!connect_with_server()) {
        return;
    }
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": Requesting \"" << request->get_request_line() << "\" from " << request->get_host() << endl;
    log_flow.close();
    int status = my_send(server_fd, my_buffer);
    if (status <= 0) {
        // request failure
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": ERROR Send Request to Server Failure" << endl;
        log_flow.close();
        return;
    }
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": NOTE Send Request to Server Successfully" << endl;
    log_flow.close();
    my_buffer.clear();
    size_t len = my_recv(server_fd, my_buffer);
    if (len <= 0) {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": ERROR Receive Response from Server Failure" << endl;
        log_flow.close();
        return;
    }
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": NOTE Receive Response from Server Successfully" << endl;
    log_flow.close();
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
    if(my_send(client_fd, my_buffer) <= 0){
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": ERROR Reply to Client Fauilure" << endl;
        log_flow.close();
        return;
    }
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": NOTE Reply to Client Successfully" << endl;
    log_flow.close();
    return;
}

bool Proxy::revalidation(){
    // construct revalidation request
    int status;
    Response *cached_resp = cache.get(request->get_full_url());
    if (cached_resp == nullptr) {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": ERROR Cached-Response Not Found" << endl;
        log_flow.close();
        return false;
    }
    std::map<std::string, std::string> *cached_resp_header = cached_resp->get_header();
    std::string newheader = "";
    auto it_etag = cached_resp_header->find("Etag");
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
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": ERROR Send Re-validation Request Failure" << endl;
        log_flow.close();
        return false;
    }
    std::vector<char> buffer(999);
    char* p = buffer.data();
    status = recv(server_fd, p, 999, 0);
    if (status <= 0) {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": ERROR Receive Re-validation Response Failure" << endl;
        log_flow.close();
        return false;
    }
    std::string s(p);
    // identify code
    string code = s.substr(s.find(" ") + 1, 3);
    if (code == "304") {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": NOTE Received 304 from Server" << endl;
        log_flow.close();
        return true;
    }
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": NOTE Received NOT 304 from Server" << endl;
    log_flow.close();
    return false;
}

//bool Proxy::can_update() {
//    std::map<std::string, std::string> *header = response_refetched->get_header();
//    auto cc = header->find("Cache-Control");
//    auto exp = header->find("Expires");
//    auto date = header->find("Date");
//    auto age = header->find("Age");
//
//    std::string cache_control;
//
//    if (cc != header->end()){
//        cache_control = cc->second;
//        if (cache_control.find("no-store") != string::npos ||
//            cache_control.find("private") != string::npos ||
//            cache_control.find("Authorization") != string::npos){
//            cerr << proxy_id << ": Cannot Cache, no-store/private/Authorization" << endl;
//            return false;
//        }
//        if(cache_control.find("max-age")!=string::npos) {
//            if (date == header->end() && age == header->end() && exp == header->end()) {
//                cerr << proxy_id << ": Cannot Cache, only max-age" << endl;
//                return false;
//            }
//        } else {
//            if (exp == header->end()){
//                cerr << proxy_id << ": Cannot Cache, no max-age and expire" << endl;
//                return false;
//            }
//        }
//    }else{
//        if(exp == header->end()){
//            cerr << proxy_id << ": Cannot Cache, no cache control and expire" << endl;
//            return false;
//        }
//    }
//    return true;
//}


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
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": not cacheable because no-store" << endl;
            log_flow.close();
            return false;
        } else if (cache_control.find("private") != string::npos) {
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": not cacheable because private" << endl;
            log_flow.close();
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
    vector<char> first_buf(65535);
    char *p = first_buf.data();
    size_t len = 0;
    if((len = recv(fd, p, 65535, 0)) <= 0){
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": ERROR my_recv() First Receive Failure" << endl;
        log_flow.close();
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
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": NOTE chunked" << endl;
        log_flow.close();
        while(true) {
            size_t once_len;
            if((once_len = continue_recv(fd)) <= 0) {
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": ERROR my_recv() chunked Loop Receive Failure" << endl;
                log_flow.close();
                return len;
            }
            len += once_len;
            vector<char> last_buf = mybuffer[mybuffer.size()-1];
            if (strstr(last_buf.data(), "0\r\n\r\n") != nullptr) {
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": NOTE last chunk arrived" << endl;
                log_flow.close();
                break;
            }
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
        // if cur_len < total_content_length + header_size
        while (len < total + header_str.size()){
            size_t once_len = 0;
            if((once_len = continue_recv(fd)) <= 0){
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": ERROR my_recv() content-length Loop Receive Failure" << endl;
                log_flow.close();
                return len;
            }
            len += once_len;
        }
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
        int once_len = send(fd, buf[sent].data(), buf[sent].size(), 0);
        if (once_len <=0){
            return once_len;
        }
        len += once_len;
        sent += 1;
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
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": NOTE only max-age, no exp or date or age" << endl;
                log_flow.close();
                return false;
            }
        } else {
            if (exp == header->end()){
                log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
                log_flow << proxy_id << ": NOTE no max-age and expire" << endl;
                log_flow.close();
                return false;
            }
        }
    }else{
        if(exp == header->end()){
            log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
            log_flow << proxy_id << ": NOTE no cache control and expire" << endl;
            log_flow.close();
            return false;
        }
    }
    return true;
}

void resp_to_buf(vector<vector<char>> &mybuffer, string response){
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
    if (len <= 0) {
        log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
        log_flow << proxy_id << ": NOTE Send Cached-Response Back Failure" << endl;
        log_flow.close();
        return;
    }
    log_flow.open(MYLOG, std::ofstream::out | std::ofstream::app);
    log_flow << proxy_id << ": NOTE Send Cached-Response Back Successfully" << endl;
    log_flow.close();
    return;
}
#endif //PROXY_PROXY_H
