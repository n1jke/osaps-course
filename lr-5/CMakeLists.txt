cmake_minimum_required(VERSION 3.16)
project(lab5 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "http_client requires Windows")
endif()

add_executable(http_client
    src/main.cpp
    src/http.cpp
    src/socket_io.cpp
    src/results.cpp
)
target_compile_features(http_client PRIVATE cxx_std_17)
set_target_properties(http_client PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(http_client PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(http_client PRIVATE ws2_32)

if(MSVC)
    target_compile_options(http_client PRIVATE /W4)
endif()
