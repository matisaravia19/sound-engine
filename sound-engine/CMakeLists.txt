cmake_minimum_required(VERSION 3.31)
project(sound-engine)

set(CMAKE_CXX_STANDARD 20)

set(SOURCES
        src/impl/vulkan.cpp
        src/engine.cpp
        src/impulse.cpp
        main.cpp)

add_library(sound-engine STATIC ${SOURCES})

find_package(Vulkan REQUIRED)
target_link_libraries(sound-engine PRIVATE Vulkan::Vulkan)

target_include_directories(sound-engine
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)