//
// Created by 徐颖 on 2/17/20.
//

#ifndef PROXY_PROXY_H
#define PROXY_PROXY_H

#include "request.h"

int buffer_size = 65535;

class Proxy {
private:
    Request* request;
    int proxy_id;
    int client_fd;
    int server_fd;
    void get_request_from_client();
    char* loop_recv(int sender_fd);
    void connect_request(); // if request.method == "CONNECT"
    void connect_with_server();
    void send_ack();

    void data_forwarding();
    void get_request(); // if request.method == "GET"
    void post_request(); // if request.method == "POST"
public:
    Proxy() {}
    Proxy(int client_fd, int id): proxy_id(id), client_fd(client_fd), server_fd(0), request(nullptr) {}
    ~Proxy() {}

    void proxy_run() {
        get_request_from_client();
        if (request->get_method() == "GET" ) {
            return;
        } else if (request->get_method() == "CONNECT") {
            connect_request();
        } else if (request->get_method() == "POST") {
            return;
        }
        std::cout << "END" << std::endl;
    }
};

void Proxy::send_ack() {
    const char *ack = "HTTP/1.1 200 OK\r\n\r\n";
    int status = send(client_fd, ack, strlen(ack), 0);
    if (status == -1) {
        std::cerr << "Error: cannot send 200 okay to client" << std::endl;
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
            cerr << "ERORR" << endl;
            break;
        }
        if (FD_ISSET(server_fd, &readfds)) {
            // recv data from server
            status = recv(server_fd, p, buffer_size, 0);
            if (status < 0) {
                cout << "status < 0" << endl;
                break;
            } else if (status == 0) {
                cerr << "Server has closed Tunnel." << endl;
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
                cout << "client status < 0" << endl;
                break;
            } else if (status == 0) {
                cerr << "Client has closed Tunnel." << endl;
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
    std::vector<char> buffer(buffer_size);
    char *p = buffer.data();
    int len = 0;
    len = recv(sender_fd, p, buffer_size, 0);
    if (len < 0) {
        exit(1);
    }
    string cur_string(buffer.begin(), buffer.end());
    int total = len;
    while (cur_string.find("\r\n\r\n") == string::npos) {
        // if not finish
        buffer.resize(total + buffer_size);
        p = buffer.data() + total;
        len = recv(sender_fd, p, buffer_size, 0);
        if (len < 0){
            exit(1);
        }
        total += len;
        cur_string = buffer.data();
    }
    return buffer.data();
    // memory leak, free buffer!
}


void Proxy::get_request_from_client() {
    // get request from client
    std::string s(loop_recv(client_fd));
    request = new Request(s, proxy_id);
    std::cout << "port ----" << request->get_port() << std::endl;
    std::cout << "host ----" << request->get_host() << std::endl;
    std::cout << "request ----" << request->get_request() << std::endl;
}

void Proxy::connect_request() {
    // connect with server
    connect_with_server();
    // send 200 OK back to client
    send_ack();
    // data transfer
    try{
        data_forwarding();
    } catch (std::exception &e) {
        cerr << "Data Forwarding Failure." << endl;
        close(server_fd);
        return;
    }
    close(server_fd);
    return;
}

void Proxy::get_request();

void Proxy::connect_with_server() {
    struct addrinfo host_info;
    struct addrinfo *p_host = &host_info;
    struct addrinfo *host_info_list;
    int status;
    memset(&host_info, 0, sizeof(host_info));//make sure the struct is empty
    p_host->ai_family   = AF_UNSPEC;
    p_host->ai_socktype = SOCK_STREAM;
    const char* hostname = request->get_host().c_str();
    const char* port = request->get_port().c_str();
    std::cout << "get_host: " << hostname << std::endl;
    std::cout << "get_port: " << port << std::endl;

    status = getaddrinfo(request->get_host().c_str(),
            request->get_port().c_str(),
            &host_info,
            &host_info_list);
    if (status != 0) {
        fprintf(stderr, "Error: cannot get address info for host\n");
        return;
    }
    server_fd = socket(host_info_list->ai_family,
                              host_info_list->ai_socktype,
                              host_info_list->ai_protocol);
    if (server_fd == -1) {
        fprintf(stderr,"Error: cannot create socket\n");
        return;
    }
    /* connect */
    status = connect(server_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        fprintf(stderr, "Error: cannot connect to socket\n");
        return;
    }
    std::cout << "Connect server successfully!" << std::endl;
}
#endif //PROXY_PROXY_H
