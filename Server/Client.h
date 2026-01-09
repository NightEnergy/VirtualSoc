#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <netinet/in.h>

class Client {
public:
    int fd;
    std::string username;
    bool isAuthenticated;
    struct sockaddr_in address;

    Client(int socket_fd, struct sockaddr_in addr) : fd(socket_fd), username(""), isAuthenticated(false), address(addr) {}

    void setUsername(const std::string& name) {
        username = name;
        isAuthenticated = true;
    }

    void logout() {
        username = "";
        isAuthenticated = false;
    }
};

#endif