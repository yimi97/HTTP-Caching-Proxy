#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include "request.h"
#include "response.h"

#define PORT "12345"  // the port users will be connecting to

#define BACKLOG 1000   // how many pending connections queue will hold

int uid = 0;

void connect_server(Request* req) {
    struct addrinfo host_info;
    struct addrinfo *p_host = &host_info;
    struct addrinfo *host_info_list;
    int status;
    int socket_fd; // proxy_client_socket_fd
    memset(p_host, 0, sizeof(host_info));//make sure the struct is empty
//    std::cout << "get_host" << hostname << ":" << req->get_host().c_str() << std::endl;
    p_host->ai_family   = AF_UNSPEC;
    p_host->ai_socktype = SOCK_STREAM;
    const char* hostname = req->get_host().c_str();
    const char* port = req->get_port().c_str();
    std::cout << "get_host: " << req->get_host().c_str() << std::endl;
    std::cout << "get_port: " << req->get_port().c_str() << std::endl;

    status = getaddrinfo(req->get_host().c_str(), req->get_port().c_str(), &host_info, &host_info_list);
    if (status != 0) {
        fprintf(stderr, "Error: cannot get address info for host\n");
        exit(1);
    } //if
    std::cout << "get addr successfull!" << std::endl;
    socket_fd = socket(host_info_list->ai_family,
                        host_info_list->ai_socktype,
                        host_info_list->ai_protocol);
    if (socket_fd == -1) {
        fprintf(stderr,"Error: cannot create socket\n");
        exit(1);
    } //if
    std::cout << "get socket successfull!" << std::endl;
    /* connect */
    status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        fprintf(stderr, "Error: cannot connect to socket\n");
        exit(1);
    } //if
    std::cout << "Connect successfull!" << std::endl;
    int len = strlen(req->get_request().c_str());
    send(socket_fd, req->get_request().c_str(), len, 0);
    std::cout << "Send successfull!" << std::endl;
    std::vector<char> buffer(65536);
    string check_end;
    char *p = buffer.data();
    int response_len = recv(socket_fd, p, 65536, 0);
    if(response_len<0){
        exit(1);
    }
    char *s = buffer.data();
    std::cout << "Here receive:" << s << std::endl;
    if (response_len > 0) {
        Response *resp = new Response(s);
    }

}

void proxy_helper(int new_fd, int uid) {
    std::vector<char> buffer(65536);
    char* p = buffer.data();
    int len = recv(new_fd, p, 65536, 0);
    if (len < 0){
        exit(1);
    }
    std::string s(p);
    Request *req = new Request(s, uid);
    std::cout << "----" << req->get_port() << std::endl;
    std::cout << "----" << req->get_host() << std::endl;
    std::cout << "----" << req->get_request() << std::endl;
    if (req->get_method() == "GET" ) {
        connect_server(req);
    } else if (req->get_method() == "CONNECT") {
        connect_server(req);
    } else if (req->get_method() == "POST") {
        connect_server(req);
    }
    std::cout << "END" << std::endl;

}

int main() {
    std::cout << "Hello, World!" << std::endl;
    const char *hostname = nullptr; // hostname set to NULL

    struct sockaddr_storage their_addr; // connector's address information
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int socket_fd, new_fd; // listen on sock_fd, new connection on new_fd

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    getaddrinfo(hostname, PORT, &hints, &res); // load up addrinfo res with getaddrinfo()

    // make a socket, bind it, and listen on it:

    socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bind(socket_fd, res->ai_addr, res->ai_addrlen);
    listen(socket_fd, BACKLOG);

    // now accept each incoming connection:
    int thread_id = 1;
    while(1){
        addr_size = sizeof their_addr;
        std::cout << "I am listening." << std::endl;
        new_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1){
            exit(1);
        }
        // create a new thread for each client connection
        proxy_helper(new_fd, uid);
        uid++;

    }

    close(socket_fd);

    return 0;
}
