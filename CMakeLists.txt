cmake_minimum_required(VERSION 3.28)
project(client C)

set(CMAKE_C_STANDARD 11)

# Globs all sources recursively
file(GLOB_RECURSE SOURCES "src/*.c")

add_executable(client ${SOURCES})
target_include_directories(client PRIVATE include)


# If on Windows, link against the required libraries
if (WIN32)




    target_link_libraries(client PRIVATE
            ws2_32
            userenv
            winhttp
            websocket
            gdi32
    )
endif ()

#If on apple, link against the required libraries, including openssl' sha256
# If on Apple, link against the required libraries
if (APPLE)
    find_library(CORE_FOUNDATION CoreFoundation)
    find_library(IO_KIT IOKit)
    find_library(CORE_TEXT CoreText)

    # Find curl
    find_package(CURL REQUIRED)

    target_link_libraries(client PRIVATE
            ${CORE_FOUNDATION}
            ${IO_KIT}
            ${CORE_TEXT}
            CURL::libcurl
    )
endif ()