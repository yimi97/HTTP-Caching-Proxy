#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include "request.h"
#include "response.h"
#include "proxy.h"
#include <arpa/inet.h>
#include <thread>

#define PORT "8080"  // the port users will be connecting to

#define BACKLOG 1000   // how many pending connections queue will hold

int client_uid = 0;


void proxy_helper(int client_fd, int uid) {
    // new a Proxy for this client
    Proxy pxy(client_fd, uid);
    pxy.proxy_run();
}

int main() {
    std::cout << "Hello, World!" << std::endl;
    const char *hostname = nullptr; // hostname set to NULL

    struct sockaddr_storage their_addr; // connector's address information
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int socket_fd, client_fd; // listen on sock_fd, new connection on new_fd

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
        try {
            client_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);
            if (client_fd == -1){
                std::cerr << "Error: Socket accept failure." << std::endl;
                return -1;
            }
        } catch (std::exception &e) {
            std::cerr << "Socket accept exception: " << e.what() << endl;
            return 0;
        }
        // create a new thread for each client connection
        std::thread t(proxy_helper, client_fd, client_uid);
        t.detach();
        client_uid++;
    }

    close(socket_fd);

    return 0;
}
