cmake_minimum_required(VERSION 3.5.0)
project(libnfs_integration_tests VERSION 0.1.0 LANGUAGES C)

set(CMAKE_BUILD_TYPE Debug)

find_package(CMocka REQUIRED)
find_package(MPI REQUIRED)
add_executable(libnfs_integration_tests 
    src/aiori-LIBNFS-test.c
    ${CMAKE_SOURCE_DIR}/../../src/aiori-LIBNFS.c)
    
target_include_directories(libnfs_integration_tests 
PRIVATE
    ${CMAKE_SOURCE_DIR}/../../src
    ${MPI_INCLUDE_PATH}
    ${LibnfsIncludePath})


target_link_directories(libnfs_integration_tests 
PRIVATE 
    ${LibnfsLibPath})

target_link_libraries(libnfs_integration_tests 
PRIVATE 
    cmocka
    nfs)