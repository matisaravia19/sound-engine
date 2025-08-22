cmake_minimum_required(VERSION 3.20)

project(sound-engine)

set(CMAKE_CXX_STANDARD 20)

set(SOURCES
        src/impl/vulkan.cpp
        src/engine.cpp
        src/impulse.cpp
        src/auralizer.cpp
        main.cpp)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

find_package(Vulkan REQUIRED)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DVK_API_VERSION_1_3)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

target_include_directories(${PROJECT_NAME} PRIVATE ../lib/VkFFT/vkFFT)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DVKFFT_BACKEND=0)

add_subdirectory(../lib/glslang ../lib/glslang)
target_include_directories(${PROJECT_NAME} PRIVATE ../lib/glslang/glslang/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glslang SPIRV OGLCompiler)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)