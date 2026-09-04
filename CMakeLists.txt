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
        # clang-cl handles __try in C++ functions natively — no /EHa needed.
        # Passing /EHa to clang-cl causes an internal compiler crash (exit 139).
        target_compile_options(BedrockUtils PRIVATE /W4 /permissive- -Qunused-arguments -Wno-invalid-offsetof)
    else()
        # MSVC cl.exe C2712: Cannot use __try in functions that require object unwinding.
        # Replace /EHsc (C++ EH only) with /EHa (async SEH + C++ EH) so that __try can
        # be used in functions that also have C++ objects with destructors.
        string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        target_compile_options(BedrockUtils PRIVATE /EHa /W4 /MP /permissive-)
    endif()
    target_link_options(BedrockUtils PRIVATE /SUBSYSTEM:WINDOWS)
endif()

set_target_properties(BedrockUtils PROPERTIES
    OUTPUT_NAME "BedrockUtils"
    PREFIX      ""
    SUFFIX      ".dll"
)
