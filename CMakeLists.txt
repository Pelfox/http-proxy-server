cmake_minimum_required(VERSION 3.20)
project(http-proxy-server LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source and header files
file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS src/*.h src/*.hpp)
list(FILTER SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

# Executable
add_executable(${PROJECT_NAME} src/main.cpp ${SOURCES} ${HEADERS})

# Project headers
target_include_directories(${PROJECT_NAME} PRIVATE src)

# External/vendor headers
target_include_directories(${PROJECT_NAME} PRIVATE include)

enable_testing()

add_executable(${PROJECT_NAME}-tests
    tests/filter_tests.cpp
    tests/vendor/catch_amalgamated.cpp
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(${PROJECT_NAME}-tests PRIVATE src include include/catch2)

add_test(NAME ${PROJECT_NAME}-tests COMMAND ${PROJECT_NAME}-tests)
