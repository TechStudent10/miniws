cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(miniws VERSION 1.0.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE include)

include(cmake/CPM.cmake)
CPMAddPackage("gh:fmtlib/fmt#11.2.0")
CPMAddPackage("gh:dankmeme01/qsox#bb3b082")
CPMAddPackage("gh:wolfSSL/wolfssl@5.8.2-stable")
# CPMAddPackage("gh:libcpr/cpr#e10e86f")

target_link_libraries(${PROJECT_NAME} PRIVATE fmt qsox wolfssl)
target_include_directories(${PROJECT_NAME} PRIVATE libs "${qsox_SOURCE_DIR}/include")

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    add_subdirectory(test)
endif()