cmake_minimum_required(VERSION 3.16)
project(osasp_lr2 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Laboratory 2 requires Windows and the Windows SDK")
endif()

add_executable(main
    src/main.cpp
    src/cli.cpp
    src/codec.cpp
    src/file_io.cpp
)
target_compile_features(main PRIVATE cxx_std_17)
set_target_properties(main PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(main PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(main PRIVATE kernel32 rpcrt4)

if(MSVC)
    target_compile_options(main PRIVATE /W4)
endif()
