#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <iostream>

class DatabaseManager {
private:
    sqlite3* db;

    // Helper pentru execuții simple
    bool executeQuery(const std::string& query) {
        char* errMsg = 0;
        if (sqlite3_exec(db, query.c_str(), 0, 0, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL Error: " << errMsg << " | Query: " << query << std::endl;
            sqlite3_free(errMsg);
            return false;
        }
        return true;
    }

public:
    DatabaseManager(const std::string& dbName) {
        if (sqlite3_open(dbName.c_str(), &db) != SQLITE_OK) {
            std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
            return;
        }

        // 1. Tabel USERS
        executeQuery("CREATE TABLE IF NOT EXISTS users ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "username TEXT UNIQUE NOT NULL, "
                     "password TEXT NOT NULL, "
                     "role INTEGER DEFAULT 0);");

        // 2. Tabel FRIENDSHIPS (Actualizat nume tabelă)
        // status: 0=Pending, 1=Accepted
        // type: 0=Normal, 1=Close Friend
        executeQuery("CREATE TABLE IF NOT EXISTS friendships ("
                     "user_id1 INTEGER, "
                     "user_id2 INTEGER, "
                     "status INTEGER DEFAULT 0, "
                     "type INTEGER DEFAULT 0, "
                     "PRIMARY KEY(user_id1, user_id2));");

        // 3. Tabel POSTS
        // visibility: 0=Public, 1=Friends, 2=Close Friends
        executeQuery("CREATE TABLE IF NOT EXISTS posts ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "user_id INTEGER, "
                     "content TEXT, "
                     "visibility INTEGER DEFAULT 0);");

        // 4. Tabel GROUPS
        executeQuery("CREATE TABLE IF NOT EXISTS groups ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "name TEXT NOT NULL, "
                     "created_by INTEGER);");

        // 5. Tabel GROUP MEMBERS
        executeQuery("CREATE TABLE IF NOT EXISTS group_members ("
                     "group_id INTEGER, "
                     "user_id INTEGER, "
                     "PRIMARY KEY (group_id, user_id), "
                     "FOREIGN KEY(group_id) REFERENCES groups(id), "
                     "FOREIGN KEY(user_id) REFERENCES users(id));");

        // 6. Tabel OFFLINE MESSAGES
        executeQuery("CREATE TABLE IF NOT EXISTS offline_messages ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "target_user_id INTEGER, "
                     "sender_name TEXT, "
                     "message_content TEXT, "
                     "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
                     "is_group_msg INTEGER DEFAULT 0, "
                     "source_group_id INTEGER DEFAULT -1);");
    }

    ~DatabaseManager() {
        sqlite3_close(db);
    }

    // --- USER MANAGEMENT ---

    bool registerUser(const std::string& username, const std::string& password, int role) {
        std::string sql = "INSERT INTO users (username, password, role) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, role);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    bool checkLogin(const std::string& username, const std::string& password) {
        std::string sql = "SELECT id FROM users WHERE username = ? AND password = ?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

        bool success = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        return success;
    }

    int getUserId(const std::string& username) {
        std::string sql = "SELECT id FROM users WHERE username = ?;";
        sqlite3_stmt* stmt;
        int id = -1;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                id = sqlite3_column_int(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
        return id;
    }

    bool isAdmin(int userId) {
        std::string sql = "SELECT role FROM users WHERE id = ?;";
        sqlite3_stmt* stmt;
        bool admin = false;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, userId);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                if (sqlite3_column_int(stmt, 0) == 1) admin = true;
            }
        }
        sqlite3_finalize(stmt);
        return admin;
    }

    bool deleteUser(const std::string& username) {
        std::string sql = "DELETE FROM users WHERE username = ?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    // --- FRIENDSHIPS (Acum folosim tabela 'friendships') ---

    bool sendFriendRequest(int fromId, int toId, int type) {
        // type: 0=Normal, 1=Close
        std::string sql = "INSERT INTO friendships (user_id1, user_id2, status, type) VALUES (?, ?, 0, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, fromId);
        sqlite3_bind_int(stmt, 2, toId);
        sqlite3_bind_int(stmt, 3, type);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    std::string getPendingRequests(int userId) {
        std::string result = "";
        std::string sql =
            "SELECT u.username, f.type FROM users u "
            "JOIN friendships f ON u.id = f.user_id1 "
            "WHERE f.user_id2 = ? AND f.status = 0;";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, userId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                int type = sqlite3_column_int(stmt, 1);
                result += name;
                if (type == 1) result += " (Close Friend Request)";
                result += "\n";
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

    bool acceptFriendRequest(int myId, int requesterId) {
        std::string sql = "UPDATE friendships SET status = 1 WHERE user_id1 = ? AND user_id2 = ? AND status = 0;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, requesterId);
        sqlite3_bind_int(stmt, 2, myId);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        if (sqlite3_changes(db) == 0) success = false;
        sqlite3_finalize(stmt);
        return success;
    }

    std::string getFriendsList(int userId) {
        std::string result = "";
        std::string sql =
            "SELECT u.username, f.type FROM users u "
            "JOIN friendships f ON (u.id = f.user_id1 OR u.id = f.user_id2) "
            "WHERE (f.user_id1 = ? OR f.user_id2 = ?) "
            "AND u.id != ? "
            "AND f.status = 1;";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, userId);
            sqlite3_bind_int(stmt, 2, userId);
            sqlite3_bind_int(stmt, 3, userId);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                int type = sqlite3_column_int(stmt, 1);

                result += name;
                if (type == 1) result += " (Close)";
                result += "\n";
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

    // --- GROUPS ---

    int createGroup(const std::string& name, int creatorId) {
        std::string sql = "INSERT INTO groups (name, created_by) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return -1;

        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, creatorId);

        int groupId = -1;
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            groupId = sqlite3_last_insert_rowid(db);
        }
        sqlite3_finalize(stmt);
        return groupId;
    }

    bool addToGroup(int groupId, int userId) {
        std::string sql = "INSERT OR IGNORE INTO group_members (group_id, user_id) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, groupId);
        sqlite3_bind_int(stmt, 2, userId);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    bool isUserInGroup(int userId, int groupId) {
        std::string sql = "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ?;";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, groupId);
        sqlite3_bind_int(stmt, 2, userId);
        bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        return exists;
    }

    std::vector<std::string> getGroupMembers(int groupId) {
        std::vector<std::string> members;
        std::string sql = "SELECT u.username FROM users u "
                          "JOIN group_members gm ON u.id = gm.user_id "
                          "WHERE gm.group_id = ?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, groupId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                members.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
            }
        }
        sqlite3_finalize(stmt);
        return members;
    }

    std::string getUserGroups(int userId) {
        std::string result = "";
        std::string sql =
            "SELECT g.id, g.name FROM groups g "
            "JOIN group_members gm ON g.id = gm.group_id "
            "WHERE gm.user_id = ?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, userId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int gid = sqlite3_column_int(stmt, 0);
                std::string gname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                result += std::to_string(gid) + ": " + gname + "\n";
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

    // --- POSTS & FEED ---

    bool createPost(int userId, const std::string& content, int visibility) {
        std::string sql = "INSERT INTO posts (user_id, content, visibility) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, userId);
        sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, visibility);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }

    bool deletePost(int postId, int userId) {
        std::string sql = "DELETE FROM posts WHERE id = ? AND user_id = ?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, postId);
        sqlite3_bind_int(stmt, 2, userId);

        sqlite3_step(stmt);
        int rowsAffected = sqlite3_changes(db);
        sqlite3_finalize(stmt);
        return rowsAffected > 0;
    }

    std::string getPostsForProfile(int myId, int targetId) {
        int relationType = -1; // -1=Nimic, 0=Friends, 1=Close
        if (myId == targetId) {
            relationType = 2;
        } else {
            std::string relSql = "SELECT type FROM friendships WHERE ((user_id1=? AND user_id2=?) OR (user_id1=? AND user_id2=?)) AND status=1;";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, relSql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, myId); sqlite3_bind_int(stmt, 2, targetId);
                sqlite3_bind_int(stmt, 3, targetId); sqlite3_bind_int(stmt, 4, myId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    relationType = sqlite3_column_int(stmt, 0);
                }
            }
            sqlite3_finalize(stmt);
        }

        std::string sql = "SELECT content, visibility FROM posts WHERE user_id = ? ORDER BY id DESC;";
        std::string result = "";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, targetId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                int vis = sqlite3_column_int(stmt, 1);

                // Filtrare
                bool canSee = false;
                if (vis == 0) canSee = true; // Public
                if (vis == 1 && relationType >= 0) canSee = true; // Friends
                if (vis == 2 && relationType >= 1) canSee = true; // Close (sau eu)

                if (canSee) {
                    std::string v = (vis==0)?"[Public]": (vis==1)?"[Friends]":"[Close]";
                    result += v + ": " + content + "\n";
                }
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

    std::string getNewsFeed(int myUserId) {
        std::string feedData = "--- News Feed ---\n";

        std::string sql =
            "SELECT u.username, p.content, p.visibility "
            "FROM posts p "
            "JOIN users u ON p.user_id = u.id "
            "WHERE "
            // 1. Public
            "   p.visibility = 0 "
            // 2. Ale mele
            "   OR p.user_id = ? "
            // 3. Friends Only (folosim friendships)
            "   OR (p.visibility = 1 AND EXISTS ( "
            "       SELECT 1 FROM friendships f "
            "       WHERE ((f.user_id1 = ? AND f.user_id2 = p.user_id) "
            "           OR (f.user_id2 = ? AND f.user_id1 = p.user_id)) "
            "       AND f.status = 1 "
            "   )) "
            // 4. Close Friends Only (folosim friendships + type=1)
            "   OR (p.visibility = 2 AND EXISTS ( "
            "       SELECT 1 FROM friendships f "
            "       WHERE ((f.user_id1 = ? AND f.user_id2 = p.user_id) "
            "           OR (f.user_id2 = ? AND f.user_id1 = p.user_id)) "
            "       AND f.status = 1 AND f.type = 1 "
            "   )) "
            "ORDER BY p.id DESC LIMIT 50;";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, myUserId);
            sqlite3_bind_int(stmt, 2, myUserId);
            sqlite3_bind_int(stmt, 3, myUserId);
            sqlite3_bind_int(stmt, 4, myUserId);
            sqlite3_bind_int(stmt, 5, myUserId);

            bool found = false;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                found = true;
                std::string author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                std::string content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                int visibility = sqlite3_column_int(stmt, 2);

                std::string visLabel = "[Public]";
                if (visibility == 1) visLabel = "[Friends]";
                if (visibility == 2) visLabel = "[Close]";

                feedData += author + " " + visLabel + ": " + content + "\n";
            }
            if (!found) feedData += "No posts yet. Add friends or post something!\n";
        } else {
             std::cerr << "SQL Error in getNewsFeed: " << sqlite3_errmsg(db) << std::endl;
        }

        sqlite3_finalize(stmt);
        return feedData;
    }

    // --- OFFLINE MESSAGES ---

    void storeOfflineMessage(int targetUserId, const std::string& senderName, const std::string& content, bool isGroup, int groupId) {
        std::string sql = "INSERT INTO offline_messages (target_user_id, sender_name, message_content, is_group_msg, source_group_id) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return;

        sqlite3_bind_int(stmt, 1, targetUserId);
        sqlite3_bind_text(stmt, 2, senderName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, isGroup ? 1 : 0);
        sqlite3_bind_int(stmt, 5, groupId);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::vector<std::string> retrieveOfflineMessages(int userId) {
        std::vector<std::string> messages;
        std::string sql = "SELECT sender_name, message_content, is_group_msg, source_group_id, timestamp FROM offline_messages WHERE target_user_id = ? ORDER BY id ASC;";
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, userId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                std::string content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                int isGroup = sqlite3_column_int(stmt, 2);
                int grpId = sqlite3_column_int(stmt, 3);
                std::string time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

                std::string formatted;
                if (isGroup) {
                    formatted = "[OFFLINE Group " + std::to_string(grpId) + " | " + sender + " @ " + time + "]: " + content + "\n";
                } else {
                    formatted = "[OFFLINE Private | " + sender + " @ " + time + "]: " + content + "\n";
                }
                messages.push_back(formatted);
            }
        }
        sqlite3_finalize(stmt);

        if (!messages.empty()) {
            std::string delSql = "DELETE FROM offline_messages WHERE target_user_id = ?;";
            sqlite3_prepare_v2(db, delSql.c_str(), -1, &stmt, 0);
            sqlite3_bind_int(stmt, 1, userId);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        return messages;
    }
};

#endif