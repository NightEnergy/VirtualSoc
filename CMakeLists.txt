cmake_minimum_required(VERSION 3.16)
project(VirtualSoc_Complete)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# --- 1. CONFIGURARE DEPENDINȚE COMUNE SI SPECIFICE ---

# Pentru Client (Qt)
find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

# Pentru Server (SQLite)
find_package(SQLite3 REQUIRED)

# --- 2. CONFIGURARE TARGET: SERVER ---

add_executable(ServerApp
        Server/main.cpp
        Server/Server.cpp
        Server/Server.h
        Server/Client.h
        Server/CommandHandler.h
        Server/Database/Database.h
)

# Serverul are nevoie de SQLite
target_link_libraries(ServerApp PRIVATE SQLite::SQLite3)
# (Optional) Dacă serverul includea path-uri relative, poate fi nevoie de:
target_include_directories(ServerApp PRIVATE Server)


# --- 3. CONFIGURARE TARGET: CLIENT (Qt) ---

add_executable(ClientApp
        Client/main.cpp
        Client/ChatWindow.h
        Client/ChatWindow.cpp
)

# Clientul are nevoie de Qt
target_link_libraries(ClientApp PRIVATE Qt6::Widgets Qt6::Network)