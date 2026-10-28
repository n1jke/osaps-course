cmake_minimum_required(VERSION 3.16)
project(lab4 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "lab4 requires Windows")
endif()

add_executable(lab4
    src/main.cpp
    src/cli.cpp
    src/sync.cpp
    src/table.cpp
)
target_compile_features(lab4 PRIVATE cxx_std_17)
set_target_properties(lab4 PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(lab4 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(lab4 PRIVATE /W4)
endif()
