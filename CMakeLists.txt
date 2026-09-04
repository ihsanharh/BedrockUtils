cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

project(BedrockUtils LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "BedrockUtils targets Windows x64. When cross-compiling on Linux, configure with:\n  cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(vendor/minhook)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_library(BedrockUtils SHARED ${SOURCES})

target_include_directories(BedrockUtils PRIVATE src)
target_link_libraries(BedrockUtils PRIVATE minhook)

target_compile_definitions(BedrockUtils PRIVATE
    UNICODE _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(BedrockUtils PRIVATE /W4 /permissive- -Qunused-arguments -Wno-invalid-offsetof)
    else()
        target_compile_options(BedrockUtils PRIVATE /W4 /MP /permissive-)
    endif()
    target_link_options(BedrockUtils PRIVATE /SUBSYSTEM:WINDOWS)
endif()

set_target_properties(BedrockUtils PROPERTIES
    OUTPUT_NAME "BedrockUtils"
    PREFIX      ""
    SUFFIX      ".dll"
)
