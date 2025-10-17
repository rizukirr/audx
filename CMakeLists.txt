cmake_minimum_required(VERSION 4.0)
project(audx VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_FLAGS_DEBUG "-Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O2")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavfilter
    libavcodec
    libavutil
    libavformat
    libswresample
    libswscale
)

if(NOT FFMPEG_FOUND)
    message(WARNING
    "FFmpeg not found!\n"
    "Please install it using:\n"
    "sudo apt install libavcodec-dev "
    "libavutil-dev "
    "libavformat-dev "
    "libswresample-dev "
    "libswscale-dev"
    )
endif()

file(GLOB_RECURSE SRC_FILES "src/*.c")
add_executable(audx main.c ${SRC_FILES})
target_include_directories(audx PUBLIC include)
target_compile_definitions(audx PRIVATE AUDX_VERSION="${PROJECT_VERSION}")

include_directories(${FFMPEG_INCLUDE_DIRS})
link_directories(${FFMPEG_LIBRARY_DIRS})

target_link_libraries(audx ${FFMPEG_LIBRARIES} m)
