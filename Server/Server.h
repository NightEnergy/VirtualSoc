#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include "Client.h"
#include "Database/Database.h"

#define MAX_EVENTS 1024
#define BUFFER_SIZE 32768
#define DISCOVERY_PORT 9001

class Server {
private:
    int server_fd;
    int udp_fd;
    int epoll_fd;
    int port;
    std::vector<Client> clients;
    struct sockaddr_in address;

    void setNonBlocking(int sock);
    void handleNewConnection();
    void handleClientActivity(int client_fd);
    void handleDiscovery();

    DatabaseManager dbManager;

public:
    Server(int port);
    ~Server();
    
    void start();
    
    // Mai mult pentru CommandHandler
    void sendMessage(int client_fd, const std::string& message);
    void broadcastMessage(const std::string& message, int exclude_fd = -1);

    // Just in case
    Client* getClient(int fd);
    void removeClient(int fd);
    Client* getClientByUsername(const std::string& username);

    DatabaseManager& getDB() { return dbManager; }
};

#endif