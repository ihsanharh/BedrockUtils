add_library(minhook STATIC
    src/buffer.c
    src/hook.c
    src/trampoline.c
    src/hde/hde32.c
    src/hde/hde64.c
)

target_include_directories(minhook PUBLIC include)

# MinHook is C code — no precompiled header, no C++ flags
set_target_properties(minhook PROPERTIES
    LINKER_LANGUAGE C
    C_STANDARD 99
)

# MinHook uses Windows APIs — suppress MSVC warnings in vendored code
if(MSVC)
    target_compile_options(minhook PRIVATE /W0)
endif()
