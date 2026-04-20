cmake_minimum_required(VERSION 3.20)
project(http-proxy-server LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source and header files
file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS src/*.h src/*.hpp)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Project headers
target_include_directories(${PROJECT_NAME} PRIVATE src)

# External/vendor headers
target_include_directories(${PROJECT_NAME} PRIVATE include)
