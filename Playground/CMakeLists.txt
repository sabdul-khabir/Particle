# Playground executable using Particle System API

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(EXE_NAME Playground)

project(${EXE_NAME})

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# GLEW, GLUT, OpenGL and TBB are found by the top-level CMakeLists.txt.

set(SOURCES
    ../DemoShared/DrawGroups.cpp
    ../DemoShared/DrawGroups.h
    ../DemoShared/Effects.cpp
    ../DemoShared/Effects.h
    ../DemoShared/Monarch.cpp
    ../DemoShared/Monarch.h
    Playground.cpp
)

source_group("src"  FILES ${SOURCES})

add_executable(${EXE_NAME} ${SOURCES})

# Playground loads sample.jpg from its working directory
set_target_properties(${EXE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_ROOT_DIR})

# This folder comes first so Effects.cpp picks up Playground's LocalUtils.h
target_include_directories(${EXE_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../DemoShared"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/stb"
)
target_link_libraries(${EXE_NAME} PRIVATE Particle GLEW::GLEW GLUT::GLUT OpenGL::GL OpenGL::GLU)

particle_copy_runtime_dlls(${EXE_NAME})
