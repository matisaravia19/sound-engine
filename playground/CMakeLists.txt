cmake_minimum_required(VERSION 3.20)
project(playground)

set(CMAKE_CXX_STANDARD 20)

add_executable(playground main.cpp)

add_subdirectory(../lib/raylib ../lib/raylib)
add_subdirectory(../sound-engine ../sound-engine)

target_link_libraries(playground PRIVATE raylib)
target_link_libraries(playground PRIVATE sound-engine)

file(COPY resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})