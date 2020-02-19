cmake_minimum_required(VERSION 3.15)
project(Proxy)

set(CMAKE_CXX_STANDARD 11)

add_executable(Proxy main.cpp request.h connect.h get.h post.h cache.h proxy.h response.h)