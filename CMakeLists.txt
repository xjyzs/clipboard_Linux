cmake_minimum_required(VERSION 3.21)
project(clipboard_Linux)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
        sio
        GIT_REPOSITORY https://github.com/socketio/socket.io-client-cpp.git
        GIT_TAG 3.1.0
        PATCH_COMMAND  git apply ${CMAKE_CURRENT_SOURCE_DIR}/patches/sio_path_fix.patch
)
FetchContent_MakeAvailable(sio)
FetchContent_Declare(
        Slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        GIT_TAG release/1
        SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)

add_compile_definitions(SIO_TLS)

find_package(OpenSSL REQUIRED)

add_executable(clipboard_Linux main.cpp
        utils/clipboard.hpp)
target_include_directories(clipboard_Linux PRIVATE ${OPENSSL_INCLUDE_DIR} ${sio_SOURCE_DIR}/src)

slint_target_sources(clipboard_Linux ui/main.slint)

target_link_libraries(clipboard_Linux PRIVATE
        sioclient_tls
        ${OPENSSL_LIBRARIES}
        pthread
        fontconfig
        Slint::Slint
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-ffunction-sections -fdata-sections)

    add_link_options(-Wl,--gc-sections)
endif()
