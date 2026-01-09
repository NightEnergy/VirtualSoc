cmake_minimum_required(VERSION 3.16)
project(VirtualSoc_Complete)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

find_package(SQLite3 REQUIRED)

add_executable(ServerApp
        Server/main.cpp
        Server/Server.cpp
        Server/Server.h
        Server/Client.h
        Server/CommandHandler.h
        Server/Database/Database.h
)

target_link_libraries(ServerApp PRIVATE SQLite::SQLite3)
target_include_directories(ServerApp PRIVATE Server)

add_executable(ClientApp
        Client/main.cpp
        Client/ChatWindow.h
        Client/ChatWindow.cpp
)

target_link_libraries(ClientApp PRIVATE Qt6::Widgets Qt6::Network)