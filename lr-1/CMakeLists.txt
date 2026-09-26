# CMakeLists.txt — сборка лабораторной №1 (вариант 26) под Windows/MSVC
# Проект: Process против Thread

cmake_minimum_required(VERSION 3.16)
project(osasp_lr1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# main.exe — родительский процесс (потоки + запуск процессов).
# Консольное приложение (выводит результаты в консоль).
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE kernel32)

# worker.exe — дочерний процесс
add_executable(worker src/worker.cpp)
target_link_libraries(worker PRIVATE kernel32)
