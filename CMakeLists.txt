cmake_minimum_required(VERSION 3.5)

project(dircomp LANGUAGES C)

add_executable(dircomp main.c)

target_link_libraries(dircomp lz4)
