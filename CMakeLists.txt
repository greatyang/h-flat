cmake_minimum_required(VERSION 2.8.6)
project(hflat CXX C)
include(ExternalProject)
set(CMAKE_CXX_FLAGS "-g --std=c++11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-unused-parameter -Wno-unused-local-typedefs -Wno-null-dereference -DGTEST_USE_OWN_TR1_TUPLE=1 -D__STDC_FORMAT_MACROS")

add_executable(hflat
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
     src/namespace/distributed_kinetic_namespace.cc
     src/namespace/simple_kinetic_namespace.cc
     src/namespace/kinetic_helper.cc
     src/namespace/threadsafe_blocking_connection.cc
     src/fsck/fsck.cc
)

set(KINETIC_CLIENT_PATH "$ENV{HOME}/git/kinetic-cpp-client/")
if(EXISTS "${KINETIC_CLIENT_PATH}")
   message("Found existing kinetic-cpp-client at ${KINETIC_CLIENT_PATH}")
else()
    message("==================================================================")
    message("Did not find existing kinetic-cpp-client at ${KINETIC_CLIENT_PATH}")
    message("will automatically download and compile a kinetic-cpp-client during make")
    message("==================================================================")
    set(KINETIC_CLIENT_PATH "${CMAKE_BINARY_DIR}/vendor/src/kinetic_cpp_client/")
ExternalProject_add(
    kinetic_cpp_client
    PREFIX "vendor"
    GIT_REPOSITORY "https://github.com/Seagate/kinetic-cpp-client"
    GIT_TAG "f68d8d8f2ba7f203f1088f622f21853526d84e2d"
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)
endif()

add_dependencies(hflat kinetic_cpp_client)
include_directories(
    ${KINETIC_CLIENT_PATH}/vendor/include
    ${KINETIC_CLIENT_PATH}/src/main/generated
    ${KINETIC_CLIENT_PATH}/src/main
    ${KINETIC_CLIENT_PATH}/include
    src/generated
    src/namespace
    src       
)
target_link_libraries(hflat
    ${KINETIC_CLIENT_PATH}/libkinetic_client.a
    ${KINETIC_CLIENT_PATH}/vendor/lib/libglog.a
    ${KINETIC_CLIENT_PATH}/vendor/lib/libgflags.a
    ${KINETIC_CLIENT_PATH}/vendor/lib/libcrypto.a
    ${KINETIC_CLIENT_PATH}/vendor/lib/libprotobuf.a
    ${CMAKE_THREAD_LIBS_INIT}
    dl
)

ExternalProject_add(
    libconfig
    PREFIX "vendor"
    URL "http://www.hyperrealm.com/libconfig/libconfig-1.4.9.tar.gz"
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<SOURCE_DIR>
    BUILD_IN_SOURCE 1
)
add_dependencies(hflat libconfig)
ExternalProject_Get_Property(libconfig SOURCE_DIR)
include_directories(${SOURCE_DIR}/include)
target_link_libraries(hflat  ${SOURCE_DIR}/lib/libconfig.a)


if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64")
    include_directories(/usr/local/include/osxfuse)
    target_link_libraries(hflat osxfuse)
else(APPLE)
    find_package(PkgConfig)
    pkg_check_modules(FUSE REQUIRED fuse)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUSE_CFLAGS_OTHER}")
    target_link_libraries(hflat ${FUSE_LIBRARIES})

    target_link_libraries(hflat pthread)
    
    ExternalProject_add(
        libuuid
        PREFIX "vendor"
        URL "http://downloads.sourceforge.net/project/libuuid/libuuid-1.0.2.tar.gz"
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<SOURCE_DIR>
        BUILD_IN_SOURCE 1
    )    
    add_dependencies(hflat libuuid)
    ExternalProject_Get_Property(libuuid SOURCE_DIR)
    include_directories(${SOURCE_DIR}/include)
    target_link_libraries(hflat ${SOURCE_DIR}/lib/libuuid.a)   
endif(APPLE)


# protobuf code generation rules
set(PROTO_GEN ${hflat_SOURCE_DIR}/src/generated)
set(PROTOC_PATH "${KINETIC_CLIENT_PATH}/vendor/host/bin/protoc")
add_custom_command(
    COMMENT "Compiling protobuf files"
    OUTPUT  ${PROTO_GEN}/database.pb.h ${PROTO_GEN}/database.pb.cc ${PROTO_GEN}/metadata.pb.h ${PROTO_GEN}/metadata.pb.cc ${PROTO_GEN}/replication.pb.h ${PROTO_GEN}/replication.pb.cc
    COMMAND mkdir -p ${PROTO_GEN} && ${PROTOC_PATH} -I=${hflat_SOURCE_DIR}/src/proto --cpp_out=${PROTO_GEN} ${hflat_SOURCE_DIR}/src/proto/*.proto 
    DEPENDS ${hflat_SOURCE_DIR}/src/proto/database.proto ${hflat_SOURCE_DIR}/src/proto/metadata.proto ${hflat_SOURCE_DIR}/src/proto/replication.proto
)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${PROTO_GEN})

add_subdirectory(tools)
add_subdirectory(iointercept)

