cmake_minimum_required(VERSION 3.16)
project(tide LANGUAGES CXX)

# Single source of truth for the version is the git tag. The release
# workflow passes -DTIDE_VERSION=x.y.z explicitly; local builds fall
# back to `git describe` (e.g. "1.0.0" on a tag, "1.0.0-3-gabc123"
# between tags). Captured at configure time.
if(NOT TIDE_VERSION)
    execute_process(
        COMMAND git describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE TIDE_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    string(REGEX REPLACE "^v" "" TIDE_VERSION "${TIDE_VERSION}")
    if(NOT TIDE_VERSION)
        set(TIDE_VERSION "unknown")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PdfWidgets)

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
    src/HtmlHighlighter.cpp
    src/CssHighlighter.cpp
    src/JsHighlighter.cpp
    src/ConfigHighlighter.cpp
    src/GherkinHighlighter.cpp
    src/DockerfileHighlighter.cpp
    src/TerraformHighlighter.cpp
    src/GoHighlighter.cpp
    src/EditorGroup.cpp
    src/Settings.cpp
    src/FileSearchDialog.cpp
    src/SymbolSearchDialog.cpp
    src/FileIconProvider.cpp
    src/IgnoreAwareModel.cpp
    src/ImageViewer.cpp
    src/PdfViewer.cpp
    resources/icons.qrc
    resources/app.qrc
)

target_link_libraries(tide PRIVATE Qt6::Widgets Qt6::PdfWidgets)
target_compile_definitions(tide PRIVATE TIDE_VERSION="${TIDE_VERSION}")

# Installed layout doubles as the .deb payload; SHLIBDEPS derives the
# package dependencies from the linked libraries (needs dpkg-dev installed).
install(TARGETS tide RUNTIME DESTINATION bin)
install(FILES resources/tide.desktop DESTINATION share/applications)
install(FILES resources/tide.svg DESTINATION share/icons/hicolor/scalable/apps)

set(CPACK_PACKAGE_NAME "tide")
set(CPACK_PACKAGE_VERSION "${TIDE_VERSION}")
set(CPACK_PACKAGE_CONTACT "Tore Martin Hagen <tore@kosli.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A lightweight Qt6 code editor")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# SHLIBDEPS only sees linked libraries, and the qsvg image plugin is
# dlopened at runtime; without it every bundled icon fails to render.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6svg6")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_GENERATOR "DEB")
include(CPack)
