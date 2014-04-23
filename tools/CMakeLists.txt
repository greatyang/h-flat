cmake_minimum_required(VERSION 2.8.6)
project(hflat-tools CXX C)

set(CMAKE_CXX_FLAGS "-g --std=c++11 -fPIC")


add_executable(hflat-tools
     tools.cc
)

target_link_libraries(hflat-tools
    dl
)
