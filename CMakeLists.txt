cmake_minimum_required(VERSION 3.16)
project(sild VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(sild
    src/main.cpp
    src/MainWindow.cpp
    src/CodeEditor.cpp
    src/CppHighlighter.cpp
)

target_link_libraries(sild PRIVATE Qt6::Widgets)
