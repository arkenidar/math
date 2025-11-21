cmake_minimum_required(VERSION 3.10)

project(math C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable debug symbols
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -Wall -Wextra")

# Add source files from src/ sources directory
file(GLOB SOURCES src/*.c)

# Add executable
add_executable(math ${SOURCES})