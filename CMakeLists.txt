cmake_minimum_required(VERSION 3.21)
project(clipboard_Linux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled dependencies as static libraries" FORCE)

set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_TESTING OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_SYSTEM_TRAY OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_ACCESSIBILITY OFF CACHE BOOL "" FORCE)

include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED_SIO ON CACHE BOOL "" FORCE)
FetchContent_Declare(
        sio
        GIT_REPOSITORY https://github.com/socketio/socket.io-client-cpp.git
        GIT_TAG 3.1.0
        PATCH_COMMAND git reset --hard
        COMMAND git apply "${CMAKE_CURRENT_SOURCE_DIR}/patches/sio_path_fix.patch"
)
FetchContent_MakeAvailable(sio)
FetchContent_Declare(
        Slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        GIT_TAG release/1
        SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)

target_link_libraries(Slint INTERFACE fontconfig)

add_compile_definitions(SIO_TLS)

find_package(OpenSSL REQUIRED)

add_executable(clipboard_Linux main.cpp
        utils/clipboard.hpp)
target_include_directories(clipboard_Linux PRIVATE ${OPENSSL_INCLUDE_DIR} ${sio_SOURCE_DIR}/src)

target_compile_options(clipboard_Linux PRIVATE
        -fvisibility=hidden
        -fvisibility-inlines-hidden
)

slint_target_sources(clipboard_Linux ui/main.slint)

target_link_libraries(clipboard_Linux PRIVATE
        sioclient_tls
        ${OPENSSL_LIBRARIES}
        pthread
        Slint::Slint
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(
            -Wl,--gc-sections
            -Wl,--as-needed
            -Wl,--build-id=none
            -Wl,-O1
    )
endif()