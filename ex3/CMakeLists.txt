cmake_minimum_required(VERSION 3.6)
project(OS3)

set(CMAKE_C_STANDARD 99)

add_executable(Server ex31.c)
add_executable(Client ex32.c)
add_executable(GameCheck reversi_logic.c)