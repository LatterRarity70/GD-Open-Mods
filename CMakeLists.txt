cmake_minimum_required(VERSION 3.21)
set(CMAKE_TLS_VERIFY 0) # Disable SSL verification because something is broken with it on Russia..
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

project(main)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
)
include_directories(src)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Include Dear ImGui library
set(IMGUI_VERSION "v1.92.0") 
CPMAddPackage("gh:LatterRarity70/gd-imgui-cocos#03c118f") # specify a commit?
target_link_libraries(${PROJECT_NAME} imgui-cocos)

setup_geode_mod(${PROJECT_NAME})
