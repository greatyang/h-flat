cmake_minimum_required(VERSION 2.8.6)
project(POSIX-o-K CXX C)

set(CMAKE_CXX_FLAGS "-g --std=c++11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-unused-parameter -Wno-unused-local-typedefs -Wno-null-dereference -DGTEST_USE_OWN_TR1_TUPLE=1 -D__STDC_FORMAT_MACROS")

include(ExternalProject)
ExternalProject_add(
    kinetic_cpp_client
    PREFIX "vendor"
    GIT_REPOSITORY "git@github.com:Seagate/Kinetic-C-Client.git"
    GIT_TAG "c146b67611ed3dcf9567e0c6160a8b28a6b83cb2"
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)

include_directories(
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/vendor/include
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/include
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/vendor/src/gmock/fused-src
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/src/main
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/src/main/generated
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/include
  
    src/generated
    src/namespace
    src       
)

set(LIBRARY_DEPENDENCIES
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/libkinetic_client.a
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/libprotobufutil.a
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/vendor/lib/libglog.a
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/vendor/lib/libgflags.a
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/vendor/lib/libssl.a
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/vendor/lib/libcrypto.a
    ${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/vendor/lib/libprotobuf.a
    ${CMAKE_THREAD_LIBS_INIT}
    dl
)


if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64")
    LIST(APPEND LIBRARY_DEPENDENCIES osxfuse)
    include_directories(/usr/local/include/osxfuse)
else(APPLE)
    find_package(PkgConfig)
    pkg_check_modules(FUSE QUIET fuse)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUSE_CFLAGS_OTHER}")
    LIST(APPEND LIBRARY_DEPENDENCIES uuid) 
    LIST(APPEND LIBRARY_DEPENDENCIES pthread)
    LIST(APPEND LIBRARY_DEPENDENCIES ${FUSE_LIBRARIES})
endif(APPLE)



# protobuf code generation rules
set(PROTO_GEN ${POSIX-o-K_SOURCE_DIR}/src/generated)
set(PROTOC_PATH "${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/vendor/src/protobufutil/vendor/host/bin/protoc")
add_custom_command(
    COMMENT "Compiling protobuf files"
    OUTPUT  {PROTO_GEN}/database.pb.h ${PROTO_GEN}/database.pb.cc {PROTO_GEN}/metadata.pb.h ${PROTO_GEN}/metadata.pb.cc {PROTO_GEN}/directorydata.pb.h ${PROTO_GEN}/directorydata.pb.cc
    COMMAND mkdir -p ${PROTO_GEN} && ${PROTOC_PATH} -I=${POSIX-o-K_SOURCE_DIR}/src/proto --cpp_out=${PROTO_GEN} ${POSIX-o-K_SOURCE_DIR}/src/proto/*.proto 
)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${PROTO_GEN})


add_executable(POSIX-o-K
	 src/main.cc
	 src/lookup.cc
	 src/metadata_info.cc
	 src/pathmapdb.cc
	 src/namespace/kinetic_namespace.cc
	 src/fuseops/attr.cc
	 src/fuseops/data.cc
	 src/fuseops/directory.cc
	 src/fuseops/file.cc
	 src/fuseops/link.cc
	 src/fuseops/permission.cc
	 src/generated/database.pb.cc
	 src/generated/directorydata.pb.cc
	 src/generated/metadata.pb.cc
)

add_dependencies(POSIX-o-K
	kinetic_cpp_client
)

target_link_libraries(POSIX-o-K
    ${LIBRARY_DEPENDENCIES}
)

add_executable(pok_test
    src/test/test_pathmapdb.cc
)
