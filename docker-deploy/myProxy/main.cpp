#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>
#include <cstring>


#define PORT "8080"  // the port users will be connecting to

#define BACKLOG 1000   // how many pending connections queue will hold

int main() {
    std::cout << "Hello, World!" << std::endl;
    const char *hostname = NULL; // hostname set to NULL

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
        std::vector<char> buffer(65536);
        char* p = buffer.data();
        int len = recv(new_fd, p, 65536, 0);
        if (len < 0){
            exit(1);
        }
        std::cout << p << std::endl;
    }

    close(socket_fd);

    return 0;
}
