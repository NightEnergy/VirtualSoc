#include "Server.h"
#include "CommandHandler.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <arpa/inet.h>
#include <algorithm>

Server::Server(int port) : port(port), dbManager("virtualsoc.db") {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) { perror("socket failed"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed"); exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    epoll_fd = epoll_create1(0);
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);
}

Server::~Server() {
    close(server_fd);
    close(epoll_fd);
}

void Server::setNonBlocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

void Server::start() {
    std::cout << "Listening on port: " << port << std::endl;
    struct epoll_event events[MAX_EVENTS];

    while (true) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == server_fd) {
                handleNewConnection();
            } else {
                handleClientActivity(events[i].data.fd);
            }
        }
    }
}

void Server::handleNewConnection() {
    int new_socket;
    int addrlen = sizeof(address);
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
        perror("accept");
        return;
    }

    setNonBlocking(new_socket);

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = new_socket;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event);

    clients.push_back(Client(new_socket, address));
    std::cout << "New connection" << std::endl;
}

void Server::handleClientActivity(int fd) {
    char buffer[BUFFER_SIZE] = {0};
    int valread = read(fd, buffer, BUFFER_SIZE - 1);

    if (valread <= 0) {
        // Clientul s-a deconectat sau eroare
        Client* c = getClient(fd);
        if(c) {
            std::cout << "Client disconnected: " << c->username << std::endl;
            broadcastMessage(c->username + " has disconnected.\n", fd);
        }
        removeClient(fd);
    } else {
        std::string raw_data(buffer);
        std::stringstream ss(raw_data);
        std::string command_line;

        Client* c = getClient(fd);
        if (c) {
            while (std::getline(ss, command_line, '\n')) {
                if (!command_line.empty() && command_line.back() == '\r') {
                    command_line.pop_back();
                }

                if (!command_line.empty()) {
                    CommandHandler::handleCommand(command_line, *c, *this);
                }
            }
        }
    }
}

void Server::sendMessage(int client_fd, const std::string& message) {
    send(client_fd, message.c_str(), message.length(), 0);
}

void Server::broadcastMessage(const std::string& message, int exclude_fd) {
    for (const auto& client : clients) {
        if (client.fd != exclude_fd) {
            sendMessage(client.fd, message);
        }
    }
}

Client* Server::getClient(int fd) {
    for (auto& client : clients) {
        if (client.fd == fd) return &client;
    }
    return nullptr;
}

Client* Server::getClientByUsername(const std::string& username) {
    for (auto& client : clients) {
        if (client.isAuthenticated && client.username == username) {
            return &client;
        }
    }
    return nullptr;
}

void Server::removeClient(int fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    clients.erase(std::remove_if(clients.begin(), clients.end(),
        [fd](const Client& c){ return c.fd == fd; }), clients.end());
}