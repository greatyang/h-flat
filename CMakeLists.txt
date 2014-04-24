cmake_minimum_required(VERSION 2.8.6)
project(hflat CXX C)

set(CMAKE_CXX_FLAGS "-g --std=c++11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-unused-parameter -Wno-unused-local-typedefs -Wno-null-dereference -DGTEST_USE_OWN_TR1_TUPLE=1 -D__STDC_FORMAT_MACROS")

include(ExternalProject)

if(EXISTS "$ENV{HOME}/git/kinetic-cpp-client/")
   message("Found existing kinetic-cpp-client at ~/git/kinetic-cpp-client.")
   set(KINETIC_CLIENT_PATH "$ENV{HOME}/git/kinetic-cpp-client/")
else()
   message("Existing kinetic-cpp-client not found. External project added.")
   set(KINETIC_CLIENT_PATH "${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/")

    ExternalProject_add(
        kinetic_cpp_client
        PREFIX "vendor"
        GIT_REPOSITORY "git@github.com:Seagate/kinetic-cpp-client.git"
        GIT_TAG ""
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
    )
endif()

include_directories(
    ${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/vendor/include
    ${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/include
    ${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/vendor/src/gmock/fused-src
    ${KINETIC_CLIENT_PATH}/src/main/generated
    ${KINETIC_CLIENT_PATH}/src/main
    ${KINETIC_CLIENT_PATH}/include
  
    src/generated
    src/namespace
    src       
)

set(LIBRARY_DEPENDENCIES
    ${KINETIC_CLIENT_PATH}/libkinetic_client.a
    ${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/libprotobufutil.a
    ${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/vendor/lib/libglog.a
    ${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/vendor/lib/libgflags.a
    ${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/vendor/lib/libssl.a
    ${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/vendor/lib/libcrypto.a
    ${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/vendor/lib/libprotobuf.a
    ${CMAKE_THREAD_LIBS_INIT}
    config
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
    LIST(APPEND LIBRARY_DEPENDENCIES pthread)
    LIST(APPEND LIBRARY_DEPENDENCIES uuid)
    LIST(APPEND LIBRARY_DEPENDENCIES ${FUSE_LIBRARIES})
endif(APPLE)


# protobuf code generation rules
set(PROTO_GEN ${hflat_SOURCE_DIR}/src/generated)
set(PROTOC_PATH "${KINETIC_CLIENT_PATH}/vendor/src/protobufutil/vendor/host/bin/protoc")
add_custom_command(
    COMMENT "Compiling protobuf files"
    OUTPUT  ${PROTO_GEN}/database.pb.h ${PROTO_GEN}/database.pb.cc ${PROTO_GEN}/metadata.pb.h ${PROTO_GEN}/metadata.pb.cc ${PROTO_GEN}/replication.pb.h ${PROTO_GEN}/replication.pb.cc
    COMMAND mkdir -p ${PROTO_GEN} && ${PROTOC_PATH} -I=${hflat_SOURCE_DIR}/src/proto --cpp_out=${PROTO_GEN} ${hflat_SOURCE_DIR}/src/proto/*.proto 
    DEPENDS ${hflat_SOURCE_DIR}/src/proto/database.proto ${hflat_SOURCE_DIR}/src/proto/metadata.proto ${hflat_SOURCE_DIR}/src/proto/replication.proto
)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${PROTO_GEN})


add_executable(hflat
     src/namespace/simple_kinetic_namespace.cc
     src/namespace/kinetic_helper.cc
     src/main.cc
     src/util.cc
     src/lookup.cc
     src/data_info.cc
     src/metadata_info.cc
     src/pathmap_db.cc
     src/fuseops/attr.cc
     src/fuseops/xattr.cc
     src/fuseops/data.cc
     src/fuseops/directory.cc
     src/fuseops/file.cc
     src/fuseops/link.cc
     src/fuseops/permission.cc
     src/fuseops/rename.cc
     src/fuseops/sync.cc
     src/generated/database.pb.cc
     src/generated/metadata.pb.cc
     src/generated/replication.pb.cc
     
     src/fsck/fsck.cc
     src/namespace/distributed_kinetic_namespace.cc
)
add_dependencies(hflat
    kinetic_cpp_client
)
target_link_libraries(hflat
    ${LIBRARY_DEPENDENCIES}
)

add_executable(hflat-tools
     tools/tools.cc
)
target_link_libraries(hflat-tools
    dl
)
add_library(iointer SHARED
     iointercept/pathchange.cc
     iointercept/iointercept.cc
)
target_link_libraries(iointer
    dl
)
