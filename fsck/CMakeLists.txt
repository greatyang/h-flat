cmake_minimum_required(VERSION 2.8.6)
project(fsck CXX C)

set(CMAKE_CXX_FLAGS "-g --std=c++11 -fPIC")


add_executable(fsck
     fsck.cc
)

target_link_libraries(fsck
    dl
)
