//
// Created by 徐颖 on 2/17/20.
//

#ifndef PROXY_PROXY_H
#define PROXY_PROXY_H

#include <sstream>
#include "request.h"
#include "cache.h"

int buffer_size = 65535;

class Proxy {
private:
    Request* request;
    int proxy_id;
    int client_fd;
    int server_fd;
    Response* response_back;
    void get_request_from_client();
    char* loop_recv(int sender_fd);
    void connect_request(); // if request.method == "CONNECT"
    bool connect_with_server();
    void send_ack();
    void data_forwarding();
    void re_fetching();
    void update_cache();
    bool reply_to_client();
    void get_request(); // if request.method == "GET"
    void post_request(); // if request.method == "POST"
public:
    Proxy() {}
    Proxy(int client_fd, int id): proxy_id(id), client_fd(client_fd), server_fd(0), request(nullptr) {}
    ~Proxy() {
        free(request);
        free(response_back);
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

char* Proxy::loop_recv(int sender_fd) {

    vector<char> *buf = new vector<char>(buffer_size);
    char *p = buf->data();
    int len = 0;
    len = recv(sender_fd, p, buffer_size, 0);
    if (len < 0) {
        exit(1);
    }
    string cur_string(buf->begin(), buf->end());
    int total = len;
    while (cur_string.find("\r\n\r\n") == string::npos) {
        // if not finish
        buf->resize(total + buffer_size);
        p = buf->data() + total;
        len = recv(sender_fd, p, buffer_size, 0);
        if (len < 0){
            exit(1);
        }
        total += len;
        cur_string = buf->data();
    }
    return buf->data();
    // memory leak, free buffer!
}

void Proxy::get_request_from_client() {
    // get request from client
    std::string s(loop_recv(client_fd));
    request = new Request(s, proxy_id);
    cout << endl << "================= " << proxy_id << " Request Received =================" << endl;
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
    Response *res = cache.get(url);
    if (res != nullptr) {
        // check if expire
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
        cout << "Mi Yi please take care of this part" << endl;
    } else {
        // Not found! Refetching! Get Response from Server
        cout << proxy_id << ": Not found! Refetching! Get Response from Server" << endl;
        re_fetching();
        // Update or Insert Cache -> update_cache()
        // Need more details like check no store, must revalidation...
        cache.put(request->get_full_url(), response_back);
        // Reply to Client
        reply_to_client();
    }
}

bool Proxy::reply_to_client(){
    int status = send(client_fd, response_back->get_response().c_str(),
                      response_back->get_response().length(), 0);
    if (status == -1) {
        cerr << proxy_id << ": Error: cannot send response back to client" << endl;
        return false;
    }
    cout << "=========" << proxy_id << ": successfully answer the client ==========" << endl;
    close(client_fd);
    return true;
}

void Proxy::update_cache() {
    string res = response_back->get_response();
    if (res.find("\r\n\r\n") == string::npos) {
        const char *ans = "HTTP/1.1 502 Bad Gateway";
        send(client_fd, ans, sizeof(ans), 0);
        // log
        free((void *) ans);
        close(client_fd);
    } else {
        cout << proxy_id << ": Good Response" << endl;
    }
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
    cout << "=========== " << proxy_id << ": Send Request Successfully in re_fetching =========" << endl;
    char *res = loop_recv(server_fd);
    response_back = new Response(res);
    cout << proxy_id << ": ************ Response ************" << endl;
    cout << res << endl;
    if (!reply_to_client()) {
        close(client_fd);
    }
    free(res);
}

void Proxy::post_request() {
    re_fetching();
//    int status;
//    server_fd = socket(AF_INET, SOCK_STREAM, 0);
//    if (server_fd == -1) {
//        cerr << "socket" << endl;
//        exit(EXIT_FAILURE);
//    }
//    struct hostent *remoteinfo = gethostbyname(request->get_host().c_str());
//    if (remoteinfo == nullptr) {
//        cerr << "host_info" << endl;
//        close(server_fd);
//        return;
//    }
//    struct sockaddr_in remote_in;
//    remote_in.sin_family = AF_INET;
//    string port;
//    if (request->get_port() == "") {
//        port = "80";
//    }
//    stringstream ss;
//    int pt;
//    ss << port;
//    ss >> pt;
//    remote_in.sin_port = htons(pt);
//    memcpy(&remote_in.sin_addr, remoteinfo->h_addr_list[0],
//           remoteinfo->h_length);
//
//    status =
//            connect(server_fd, (struct sockaddr *)&remote_in, sizeof(remote_in));
//    if (status == -1) {
//        cerr << "connect" << endl;
//        close(server_fd);
//        return;
//    }
//    cout << proxy_id << ": connect server successfully" << endl;
//    const char *request_tosend = request->get_request().c_str();
//
//    cout << proxy_id << ": server = " << server_fd << " request: " << request_tosend << endl;
//    status = send(server_fd, request_tosend, strlen(request_tosend), 0);
//
//    if (status == -1) {
//        cerr << "send" << endl;
//        close(server_fd);
//        return;
//    }
//    cout << proxy_id << ": send to server successfully" << endl;
//    vector<char> *newbuffer;
//    try {
//        newbuffer = new vector<char>(2000);
//    } catch (std::bad_alloc &ba) {
//        std::cerr << "Bad alloc " << ba.what() << endl;
//        close(server_fd);
//        return;
//    }
//
//    long int total = 0;
//    long int headerlen = 0;
//    long int recvlen;
//    long int size = 2000;
//    long int response_content_length = -1;
//    string response_header;
//    char *p = newbuffer->data();
//    recvlen = recv(server_fd, p, size, 0);
//    total = recvlen;
//    cout << proxy_id << ": receive " << recvlen << " bytes from server" << endl;
//    string tmp(newbuffer->begin(), newbuffer->end());
//    size_t headerend = tmp.find("\r\n\r\n");
//    if (headerend != string::npos) {
//        response_header = tmp.substr(0, headerend + 4);
//        headerlen = response_header.size();
//
//    } else {
//        const char *badreqres = "HTTP/1.1 502 Bad Gateway";
//        send(client_fd, badreqres, sizeof(badreqres), 0);
//        free(newbuffer);
//        close(server_fd);
//        return;
//    }
}

#endif //PROXY_PROXY_H
