cmake_minimum_required(VERSION 2.8.6)
project(poktools CXX C)

set(CMAKE_CXX_FLAGS "-g --std=c++11 -fPIC")


add_executable(poktools
     tools.cc
)

target_link_libraries(poktools
    dl
)
