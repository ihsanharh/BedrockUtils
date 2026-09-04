set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)
set(CMAKE_RANLIB llvm-ranlib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)

# Resolve MSVC sysroot (CRT & Windows SDK)
# Priority:
#  1. -DXWIN_DIR=<path> passed via CMake
#  2. Environment variable XWIN_DIR
#  3. Standard fallback locations
if(NOT DEFINED XWIN_DIR)
    if(DEFINED ENV{XWIN_DIR} AND IS_DIRECTORY "$ENV{XWIN_DIR}")
        set(XWIN_DIR "$ENV{XWIN_DIR}")
    elseif(DEFINED ENV{HOME} AND IS_DIRECTORY "$ENV{HOME}/Projects/msvc_sysroot")
        set(XWIN_DIR "$ENV{HOME}/Projects/msvc_sysroot")
    elseif(DEFINED ENV{HOME} AND IS_DIRECTORY "$ENV{HOME}/msvc_sysroot")
        set(XWIN_DIR "$ENV{HOME}/msvc_sysroot")
    elseif(IS_DIRECTORY "/msvc_sysroot")
        set(XWIN_DIR "/msvc_sysroot")
    else()
        message(FATAL_ERROR 
            "MSVC sysroot (XWIN_DIR) not found.\n"
            "Please configure with: cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake -DXWIN_DIR=/path/to/msvc_sysroot\n"
            "Or set the XWIN_DIR environment variable."
        )
    endif()
endif()

message(STATUS "Using MSVC sysroot at: ${XWIN_DIR}")

set(CLANG_FLAGS "/vctoolsdir \"${XWIN_DIR}/crt\" /winsdkdir \"${XWIN_DIR}/sdk\" -Xclang --dependent-lib=msvcrt")

set(CMAKE_C_FLAGS_INIT "${CLANG_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CLANG_FLAGS}")

set(LINK_FLAGS "/libpath:\"${XWIN_DIR}/crt/lib/x64\" /libpath:\"${XWIN_DIR}/sdk/lib/ucrt/x64\" /libpath:\"${XWIN_DIR}/sdk/lib/um/x64\" /nodefaultlib:msvcrtd")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${LINK_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${LINK_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${LINK_FLAGS}")
