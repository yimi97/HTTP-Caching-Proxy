#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
int main() {
    std::cout << "Hello, World!" << std::endl;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int socket_fd, new_fd;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    getaddrinfo(NULL, MYPORT, &hints, &res);

    // make a socket, bind it, and listen on it:

    socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bind(socket_fd, res->ai_addr, res->ai_addrlen);
    listen(socket_fd, BACKLOG);

    // now accept an incoming connection:
    while(1){
        addr_size = sizeof their_addr;
        new_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1){
            exit(1);
        }
        
    }

    close(socket_fd);

    return 0;
}
