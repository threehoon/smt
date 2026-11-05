cmake_minimum_required(VERSION 3.16)

project(smtpro VERSION 0.1.0 LANGUAGES CXX)

# Product name shown to users. Targets and libraries keep the smtpro prefix.
set(SMTPRO_DISPLAY_NAME "迅贴 SwiftPlace")
set(SMTPRO_EXE_NAME "SwiftPlace")
set(SMTPRO_PUBLISHER "SwiftPlace")

if(NOT MSVC)
    message(FATAL_ERROR
        "smtpro must be built with MSVC 14.44 (v143). Got ${CMAKE_CXX_COMPILER_ID}. "
        "Run dev-env.bat first; PATH currently prefers MinGW.")
endif()
if(MSVC_VERSION GREATER_EQUAL 1950)
    message(FATAL_ERROR
        "MSVC ${CMAKE_CXX_COMPILER_VERSION} cannot compile Qt 5.14.2 headers "
        "(stdext::make_checked_array_iterator removed). Use toolset 14.44.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui Qml Quick Test)
message(STATUS "Qt5: ${Qt5_VERSION} (${Qt5_DIR})")

# Compile options for our own code. Qt headers come in through imported targets,
# which CMake treats as system includes, so /W4 only applies to our sources.
add_library(smtpro_options INTERFACE)
target_compile_options(smtpro_options INTERFACE /utf-8 /permissive- /Zc:__cplusplus /W4)
target_compile_definitions(smtpro_options INTERFACE UNICODE _UNICODE
    # QVector/QList operator== in Qt 5.14 headers use stdext::checked_array_iterator,
    # which MSVC 14.44 marks deprecated (C4996, STL4043). Silences only that warning.
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)

add_subdirectory(src)
# After src, so its install() rules put the exe in place before windeployqt runs.
add_subdirectory(packaging)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
