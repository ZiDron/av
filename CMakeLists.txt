cmake_minimum_required(VERSION 3.14)

project(av LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

link_libraries(
    avformat
    avcodec
    swscale
    avutil
)

add_executable(av
  main.cpp
)
