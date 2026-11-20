# Benchmark executable using Particle System API
# It runs the same effects as Playground but draws nothing, so it needs no GLEW, GLUT or OpenGL.

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(EXE_NAME Benchmark)

project(${EXE_NAME})

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(SOURCES
    ../DemoShared/Effects.cpp
    ../DemoShared/Effects.h
    Benchmark.cpp
)

source_group("src"  FILES ${SOURCES})

add_executable(${EXE_NAME} ${SOURCES})

set_target_properties(${EXE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_ROOT_DIR})

# This folder comes first so Effects.cpp picks up Benchmark's LocalUtils.h
target_include_directories(${EXE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(${EXE_NAME} PRIVATE Particle)

particle_copy_runtime_dlls(${EXE_NAME})
