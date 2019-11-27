cmake_minimum_required(VERSION 3.15)
project(NetworksLab2019 C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

set(CMAKE_C_FLAGS "-g -pthread")

add_executable(my_client_linux
        tcp/client/main.c tcp/structs.h)

add_executable(my_server_linux
        tcp/server/main.c tcp/structs.h)