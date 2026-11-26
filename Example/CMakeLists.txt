# Example executable using Particle System API

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(EXE_NAME Example)

project(${EXE_NAME})

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# GLEW, GLUT, OpenGL and TBB are found by the top-level CMakeLists.txt.

set(SOURCES Example.cpp)

source_group("src"  FILES ${SOURCES})

add_executable(${EXE_NAME} ${SOURCES})

set_target_properties(${EXE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_ROOT_DIR})

target_link_libraries(${EXE_NAME} PRIVATE Particle GLEW::GLEW GLUT::GLUT OpenGL::GL OpenGL::GLU)

particle_copy_runtime_dlls(${EXE_NAME})
