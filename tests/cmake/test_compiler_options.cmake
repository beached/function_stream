set(DAW_JSON_USE_SANITIZERS OFF CACHE STRING "Enable address and undefined sanitizers")
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
    if (MSVC)
        message("Clang-CL ${CMAKE_CXX_COMPILER_VERSION} detected")
        add_definitions(-DNOMINMAX -DD_WIN32_WINNT=0x0601)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /O0 -DDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 -DNDEBUG")
        if (DAW_ALLOW_SSE42)
            message("Using -march=native")
            add_compile_options(-march=native)
        endif ()
    else ()
        message("Clang ${CMAKE_CXX_COMPILER_VERSION} detected")
        add_compile_options(
                -Wall
                -Wextra
                -pedantic
                -Weverything
                -ftemplate-backtrace-limit=0
                -Wno-c++98-compat
                -Wno-c++98-compat-pedantic
                -Wno-covered-switch-default
                -Wno-ctad-maybe-unsupported
                -Wno-disabled-macro-expansion
                -Wno-documentation
                -Wno-exit-time-destructors
                -Wno-float-equal
                -Wno-global-constructors
                -Wno-missing-braces
                -Wno-missing-noreturn
                -Wno-missing-prototypes
                -Wno-newline-eof
                -Wno-padded
                -Wno-tautological-type-limit-compare
                -Wno-unneeded-internal-declaration
                -Wno-unused-local-typedefs
                -Wno-unused-macros
                -Wno-unused-parameter
                -Wunreachable-code
                -Wzero-as-null-pointer-constant
        )
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
            if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
                if (CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL 10.0.0)
                    message("Adding Intel JCC bugfix")
                    add_compile_options(-mbranches-within-32B-boundaries)
                endif ()
            endif ()
        endif ()
        if (DAW_ALLOW_SSE42)
            message("Using -march=native")
            add_compile_options(-march=native)
        endif ()
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g -DNDEBUG")

        if (DAW_JSON_USE_SANITIZERS)
            message("Using sanitizers")
            #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=null")
            #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fsanitize=null")
            #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fsanitize=undefined")
            #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fsanitize=address")
            #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined")
            #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fsanitize=thread")

        endif ()
        if (CMAKE_BUILD_TYPE STREQUAL "coverage" OR CODE_COVERAGE)
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-instr-generate -fcoverage-mapping")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-instr-generate -fcoverage-mapping")
        endif ()
    endif ()
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    message("g++ ${CMAKE_CXX_COMPILER_VERSION} detected")
    add_compile_options(
            -Wall
            -Wextra
            -pedantic
            -Wdisabled-optimization
            -Wduplicated-cond
            -Wlogical-op
            -Wno-deprecated-declarations
            -Wno-unused-local-typedefs
            -Wold-style-cast
            -Wshadow
            -Wzero-as-null-pointer-constant
    )
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
        if (CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL 9.0.0)
            if (LINUX)
                message("Adding Intel JCC bugfix")
                add_compile_options(-Wa,-mbranches-within-32B-boundaries)
            endif ()
        endif ()
    endif ()
    if (DAW_ALLOW_SSE42)
        message("Using -march=native")
        add_compile_options(-march=native)
    endif ()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

    if (DAW_JSON_USE_SANITIZERS)
        message("Using sanitizers")
        #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fsanitize=undefined")
        #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fsanitize=address")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
        #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
        #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
    endif ()
elseif (MSVC)
    message("MSVC detected")
    add_definitions(-DNOMINMAX -DD_WIN32_WINNT=0x0601)
    add_compile_options("/permissive-")
    add_compile_options("/wd4146")
    add_compile_options("/bigobj")
else ()
    message("Unknown compiler id ${CMAKE_CXX_COMPILER_ID}")
endif ()

