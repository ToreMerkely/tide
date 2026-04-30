cmake_minimum_required(VERSION 3.16)
project(tide VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(tide
    src/main.cpp
    src/MainWindow.cpp
    src/CodeEditor.cpp
    src/CppHighlighter.cpp
    src/LspClient.cpp
    src/SearchBar.cpp
    src/PythonHighlighter.cpp
    src/JsonHighlighter.cpp
    src/YamlHighlighter.cpp
    src/ShellHighlighter.cpp
    src/MakefileHighlighter.cpp
    src/MarkdownHighlighter.cpp
    src/Settings.cpp
    src/FileSearchDialog.cpp
    src/SymbolSearchDialog.cpp
    src/FileIconProvider.cpp
    resources/icons.qrc
)

target_link_libraries(tide PRIVATE Qt6::Widgets)
