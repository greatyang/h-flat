cmake_minimum_required(VERSION 2.8.6)
project(iointercept CXX C)

set(CMAKE_CXX_FLAGS "-g --std=c++11 -fPIC")


add_library(iointer SHARED
     pathchange.cc
     iointercept.cc
)

target_link_libraries(iointer
    dl
)