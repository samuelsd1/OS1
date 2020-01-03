cmake_minimum_required(VERSION 3.6)
project(OS3)

set(CMAKE_C_STANDARD 99)



add_executable(Client ex41.c)
target_link_libraries(Client pthread)
add_executable(Server ex42.c)
target_link_libraries(Server pthread)