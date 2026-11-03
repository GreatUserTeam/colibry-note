cmake_minimum_required(VERSION 3.21)

project(colibry-notes
    VERSION 0.2.0
    DESCRIPTION "Notes for the Colibry"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(Colibry_BUILD_TESTS "Build unit tests" ON)
option(Colibry_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(_qt_components Widgets)
if(Colibry_BUILD_TESTS)
    list(APPEND _qt_components Test)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS ${_qt_components})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(ColibryApps)

if(Colibry_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(common)
add_library(gus_notes_core STATIC
    src/core/NoteStore.h
    src/core/NoteStore.cpp
)
target_include_directories(gus_notes_core PUBLIC src)
target_link_libraries(gus_notes_core PUBLIC Qt6::Widgets)
colibry_target_defaults(gus_notes_core)

add_library(gus_notes_ui STATIC
    src/NotesWindow.h
    src/NotesWindow.cpp
)
target_include_directories(gus_notes_ui PUBLIC src)
target_link_libraries(gus_notes_ui PUBLIC gus_notes_core colibry_common Qt6::Widgets)
colibry_target_defaults(gus_notes_ui)

add_executable(colibry-notes src/main.cpp)
target_link_libraries(colibry-notes PRIVATE gus_notes_ui)
colibry_target_defaults(colibry-notes)

colibry_install_app(colibry-notes
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-notes.desktop
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-notes.svg)

if(Colibry_BUILD_TESTS)
    colibry_add_test(notes_core SOURCES tests/test_core.cpp LINK gus_notes_core)
    colibry_add_test(notes_ui SOURCES tests/test_ui.cpp LINK gus_notes_ui)
endif()
