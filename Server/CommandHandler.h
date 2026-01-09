#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <sstream>
#include <iostream>
#include "Server.h"

class CommandHandler {
public:
    static void handleCommand(std::string raw_command, Client& client, Server& server) {
        if (!raw_command.empty() && raw_command.back() == '\n') raw_command.pop_back();
        if (!raw_command.empty() && raw_command.back() == '\r') raw_command.pop_back();

        std::stringstream ss(raw_command);
        std::string command;
        ss >> command;

        // public commands

        if (command == "REGISTER") {
            // REGISTER <username> <password> <role>
            std::string username, password;
            int role;
            ss >> username >> password >> role; // role: 0=user, 1=admin

            if (username.empty() || password.empty() || ss.fail()) {
                server.sendMessage(client.fd, "400 Bad Request: Format is REGISTER <user> <pass> <role>\n");
                return;
            }
            if (server.getDB().registerUser(username, password, role)) {
                server.sendMessage(client.fd, "201 Created: User registered.\n");
            } else {
                server.sendMessage(client.fd, "409 Conflict: Username already exists.\n");
            }
        }
        else if (command == "LOGIN") {
            // LOGIN <username> <password>
            std::string username, password;
            ss >> username >> password;

            if (client.isAuthenticated) {
                server.sendMessage(client.fd, "400 Bad Request: Already logged in.\n");
                return;
            }
            if (server.getDB().checkLogin(username, password)) {
                client.setUsername(username);
                server.sendMessage(client.fd, "200 OK: Welcome " + username + "!\n");
            } else {
                server.sendMessage(client.fd, "401 Unauthorized: Wrong user or pass.\n");
            }

            int myId = server.getDB().getUserId(username);
            std::vector<std::string> pendingMsgs = server.getDB().retrieveOfflineMessages(myId);
            if (!pendingMsgs.empty()) {
                server.sendMessage(client.fd, "\n--- You received messages while offline ---\n");
                for (const auto& msg : pendingMsgs) {
                    server.sendMessage(client.fd, msg);
                }
                server.sendMessage(client.fd, "-------------------------------------------\n");
            }
        }
        else if (command == "VIEW_POSTS") {
            // VIEW_POSTS <username>
            std::string targetUser;
            ss >> targetUser;

            int targetId = server.getDB().getUserId(targetUser);
            int myId = client.isAuthenticated ? server.getDB().getUserId(client.username) : -1;

            if (targetId == -1) {
                server.sendMessage(client.fd, "404 User not found.\n");
                return;
            }

            std::string posts = server.getDB().getPostsForProfile(myId, targetId);
            server.sendMessage(client.fd, "--- Posts for " + targetUser + " ---\n" + posts + "----------------------\n");
        }
        else if (command == "FEED") {
            // FEED
            int myId = server.getDB().getUserId(client.username);
            std::string feed = server.getDB().getNewsFeed(myId);
            server.sendMessage(client.fd, feed);
        }

        // user commands
        else if (command == "LOGOUT") {
            // LOGOUT
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden: Login required.\n"); return; }

            server.broadcastMessage(client.username + " has disconnected.\n", client.fd);
            client.logout();
            server.sendMessage(client.fd, "200 OK: Logged out.\n");
        }
        else if (command == "ADD_FRIEND") {
            // ADD_FRIEND <username> <type>
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden: Login required.\n"); return; }

            std::string targetUser, typeStr;
            ss >> targetUser >> typeStr;
            int targetId = server.getDB().getUserId(targetUser);
            int myId = server.getDB().getUserId(client.username);

            if (targetId == -1) {
                server.sendMessage(client.fd, "404 Not Found.\n");
                return;
            }
            int type = (typeStr == "close") ? 1 : 0;
            if (server.getDB().sendFriendRequest(myId, targetId, type)) {
                server.sendMessage(client.fd, "200 OK: Friend request sent.\n");
            } else {
                server.sendMessage(client.fd, "400 Error: Request failed (already friends/pending?).\n");
            }
        }
        else if (command == "VIEW_REQUESTS") {
            // VIEW_REQUESTS
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden: Login required.\n"); return; }

            int myId = server.getDB().getUserId(client.username);
            std::string reqs = server.getDB().getPendingRequests(myId);
            server.sendMessage(client.fd, "--- Friend Requests ---\n" + reqs);
        }
        else if (command == "ACCEPT_REQUEST") {
            // ACCEPT_REQUEST <username>
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden: Login required.\n"); return; }

            std::string requesterUser;
            ss >> requesterUser;
            int requesterId = server.getDB().getUserId(requesterUser);
            int myId = server.getDB().getUserId(client.username);

            if (server.getDB().acceptFriendRequest(myId, requesterId)) {
                server.sendMessage(client.fd, "200 OK: Request accepted.\n");
            } else {
                server.sendMessage(client.fd, "400 Error: No pending request found.\n");
            }
        }
        else if (command == "POST") {
            // POST <visibility> <content...>
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden: Login required.\n"); return; }

            std::string visibilityStr, content;
            ss >> visibilityStr;
            std::getline(ss, content);
            if (!content.empty() && content[0] == ' ') content.erase(0, 1);

            if (content.empty()) {
                server.sendMessage(client.fd, "400 Empty post.\n");
                return;
            }

            int visibility = 0;
            if (visibilityStr == "friends") visibility = 1;
            else if (visibilityStr == "close") visibility = 2;

            int myId = server.getDB().getUserId(client.username);

            // DEBUG: Afișează în consola serverului
            std::cout << "User " << client.username << " is posting: " << content << " (Vis: " << visibility << ")" << std::endl;

            if(server.getDB().createPost(myId, content, visibility)) {
                server.sendMessage(client.fd, "201 Created.\n");
            } else {
                server.sendMessage(client.fd, "500 Server Error: Could not save post.\n");
            }
        }
        else if (command == "MSG") {
            // MSG <username> <msg...>
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden: Login required.\n"); return; }

            std::string destUser, msgContent;
            ss >> destUser;
            std::getline(ss, msgContent);
            if (!msgContent.empty() && msgContent[0] == ' ') msgContent.erase(0, 1);

            Client* destClient = server.getClientByUsername(destUser);
            if (destClient) {
                // ONLINE
                std::string formattedMsg = "[Private from " + client.username + "]: " + msgContent + "\n";
                server.sendMessage(destClient->fd, formattedMsg);
                server.sendMessage(client.fd, "200 OK: Sent.\n");
            } else {
                // OFFLINE
                int targetId = server.getDB().getUserId(destUser);
                if (targetId != -1) {
                    server.getDB().storeOfflineMessage(targetId, client.username, msgContent, false, -1);
                    server.sendMessage(client.fd, "200 OK: User offline. Message saved.\n");
                } else {
                    server.sendMessage(client.fd, "404 User does not exist.\n");
                }
            }
        }
        else if (command == "CREATE_GROUP") {
            // CREATE_GROUP <nume_grup>
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden\n"); return; }

            std::string groupName;
            std::getline(ss, groupName);
            if (!groupName.empty() && groupName[0] == ' ') groupName.erase(0, 1);

            if (groupName.empty()) {
                server.sendMessage(client.fd, "400 Name required.\n");
                return;
            }

            int myId = server.getDB().getUserId(client.username);
            int groupId = server.getDB().createGroup(groupName, myId);

            if (groupId != -1) {
                server.getDB().addToGroup(groupId, myId);
                server.sendMessage(client.fd, "200 OK: Group '" + groupName + "' created with ID " + std::to_string(groupId) + ".\n");
            } else {
                server.sendMessage(client.fd, "500 Server Error.\n");
            }
        }

        else if (command == "ADD_TO_GROUP") {
            // ADD_TO_GROUP <group_id> <username_de_adaugat>
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden\n"); return; }

            int groupId;
            std::string newMemberUser;
            ss >> groupId >> newMemberUser;

            int myId = server.getDB().getUserId(client.username);

            if (!server.getDB().isUserInGroup(myId, groupId)) {
                server.sendMessage(client.fd, "403 You are not in this group.\n");
                return;
            }

            int newMemberId = server.getDB().getUserId(newMemberUser);
            if (newMemberId == -1) {
                server.sendMessage(client.fd, "404 User not found.\n");
                return;
            }

            if (server.getDB().addToGroup(groupId, newMemberId)) {
                server.sendMessage(client.fd, "200 OK: User added.\n");

                Client* destClient = server.getClientByUsername(newMemberUser);
                if (destClient) {
                    server.sendMessage(destClient->fd, "Info: You were added to group ID " + std::to_string(groupId) + " by " + client.username + ".\n");
                }
            } else {
                server.sendMessage(client.fd, "400 Error (maybe already inside?).\n");
            }
        }

        else if (command == "GROUP_MSG") {
            // GROUP_MSG <group_id> <mesaj...>
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden\n"); return; }

            int groupId;
            ss >> groupId;

            std::string msgContent;
            std::getline(ss, msgContent);
            if (!msgContent.empty() && msgContent[0] == ' ') msgContent.erase(0, 1);

            int myId = server.getDB().getUserId(client.username);

            if (!server.getDB().isUserInGroup(myId, groupId)) {
                server.sendMessage(client.fd, "403 You are not in this group.\n");
                return;
            }

            std::vector<std::string> members = server.getDB().getGroupMembers(groupId);

            std::string formattedMsg = "[Group " + std::to_string(groupId) + "] " + client.username + ": " + msgContent + "\n";

            for (const auto& memberName : members) {
                if (memberName == client.username) continue;

                Client* destClient = server.getClientByUsername(memberName);
                if (destClient) {
                    // ONLINE
                    server.sendMessage(destClient->fd, formattedMsg);
                } else {
                    // OFFLINE
                    int targetId = server.getDB().getUserId(memberName);
                    if (targetId != -1) {
                        server.getDB().storeOfflineMessage(targetId, client.username, msgContent, true, groupId);
                    }
                }
            }
            server.sendMessage(client.fd, "200 OK: Sent to group (stored for offline members).\n");
        }
        else if (command == "VIEW_FRIENDS") {
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden\n"); return; }

            int myId = server.getDB().getUserId(client.username);
            std::string friends = server.getDB().getFriendsList(myId);

            server.sendMessage(client.fd, "--- Friends List ---\n" + friends);
        }
        else if (command == "VIEW_GROUPS") {
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden\n"); return; }

            int myId = server.getDB().getUserId(client.username);
            std::string groups = server.getDB().getUserGroups(myId);

            server.sendMessage(client.fd, "--- Groups List ---\n" + groups);
        }

        // admin commands
        else if (command == "DELETE_USER") {
            // DELETE_USER <username>
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden: Login required.\n"); return; }

            int myId = server.getDB().getUserId(client.username);
            if (!server.getDB().isAdmin(myId)) {
                server.sendMessage(client.fd, "403 Forbidden: Admin access required.\n");
                return;
            }

            std::string targetUser;
            ss >> targetUser;

            if (server.getDB().deleteUser(targetUser)) {
                server.sendMessage(client.fd, "200 OK: User " + targetUser + " deleted.\n");

                Client* targetClient = server.getClientByUsername(targetUser);
                if (targetClient) {
                     server.sendMessage(targetClient->fd, "You have been banned/deleted by admin.\n");
                     targetClient->logout();
                }
            } else {
                server.sendMessage(client.fd, "404 User not found or error deleting.\n");
            }
        }
        else if (command == "DELETE_POST") {
            // DELETE_POST <id>
            if (!client.isAuthenticated) { server.sendMessage(client.fd, "403 Forbidden: Login required.\n"); return; }

            int postId;
            ss >> postId;

            if (ss.fail()) {
                server.sendMessage(client.fd, "400 Bad Request: Invalid ID format.\n");
                return;
            }

            int myId = server.getDB().getUserId(client.username);

            if (server.getDB().deletePost(postId, myId)) {
                server.sendMessage(client.fd, "200 OK: Post " + std::to_string(postId) + " deleted.\n");
            } else {
                server.sendMessage(client.fd, "403 Forbidden or Not Found: You can only delete your own posts.\n");
            }
        }
        else {
            server.sendMessage(client.fd, "400 Unknown Command.\n");
        }
    }
};

#endif