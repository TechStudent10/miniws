cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(miniws VERSION 1.0.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE include)

include(cmake/CPM.cmake)
CPMAddPackage(
    NAME fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt.git"
    GIT_TAG "11.2.0"
    OPTIONS "BUILD_SHARED_LIBS OFF"
)
CPMAddPackage("gh:dankmeme01/qsox#93da9bfc")
CPMAddPackage("gh:geode-sdk/result#v1.3.4")
CPMAddPackage(
    NAME wolfssl
    GIT_REPOSITORY "https://github.com/wolfSSL/wolfssl.git"
    GIT_TAG "v5.8.2-stable"
    OPTIONS "WOLFSSL_QUIC OFF"
            "WOLFSSL_HARDEN OFF"
            "WOLFSSL_OPENSSLEXTRA ON"
            "WOLFSSL_TLSV12 ON"
            "WOLFSSL_INSTALL OFF"
            "WOLFSSL_CRYPT_TESTS OFF"
            "WOLFSSL_EXAMPLES OFF"
            "BUILD_SHARED_LIBS OFF"
)
# CPMAddPackage("gh:libcpr/cpr#e10e86f")

target_link_libraries(${PROJECT_NAME} PRIVATE fmt qsox wolfssl)
target_link_libraries(${PROJECT_NAME} PUBLIC GeodeResult)
target_include_directories(${PROJECT_NAME} PRIVATE libs "${qsox_SOURCE_DIR}/include")

# silence wolfssl warnings

if (MSVC)
    target_compile_options(wolfssl PRIVATE /w)
else()
    target_compile_options(wolfssl PRIVATE -w)
endif()

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    add_subdirectory(test)
endif()