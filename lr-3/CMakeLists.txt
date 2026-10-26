cmake_minimum_required(VERSION 3.16)
project(osasp_lr3 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Laboratory 3 requires Windows and the Windows SDK")
endif()

add_executable(lr3
    src/main.cpp
    src/cli.cpp
    src/protocol.cpp
    src/mailslot.cpp
)
target_compile_features(lr3 PRIVATE cxx_std_17)
set_target_properties(lr3 PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(lr3 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(lr3 PRIVATE kernel32)

if(MSVC)
    target_compile_options(lr3 PRIVATE /W4 /permissive-)
else()
    target_compile_options(lr3 PRIVATE -Wall -Wextra -Wpedantic)
endif()
