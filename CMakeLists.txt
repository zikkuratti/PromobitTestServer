project(TCPServerWithLiburing)

cmake_minimum_required(VERSION 3.10)



add_executable(server server.c)

target_link_libraries(server PUBLIC uring)

