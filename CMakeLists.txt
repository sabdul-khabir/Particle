# Particle System API Main Solution

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(EXE_NAME ParticleSolution)

project(${EXE_NAME})

set(CMAKE_CXX_STANDARD 17)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(TBB REQUIRED)
if(MSVC)
    # build in parallel (MSVC-specific)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/MP>)
else()
    # On MinGW/MSYS2, avoid searching for MSVC-specific debug suffixes like 'd'
    # which often leads to errors like 'freeglut_staticd.lib' not found.
    set(GLUT_ADDITIONAL_LIB_NAMES freeglut freeglut_static)
    
    # If building in Debug mode on MinGW, libraries often don't have a 'd' suffix.
    # This helps find_package(GLEW) and others succeed.
    set(CMAKE_DEBUG_POSTFIX "")
endif()


add_subdirectory(${PROJECT_ROOT_DIR}/ParticleLib ${CMAKE_CURRENT_BINARY_DIR}/ParticleLib)

add_subdirectory(${PROJECT_ROOT_DIR}/Benchmark ${CMAKE_CURRENT_BINARY_DIR}/Benchmark)
add_subdirectory(${PROJECT_ROOT_DIR}/Example ${CMAKE_CURRENT_BINARY_DIR}/Example)
add_subdirectory(${PROJECT_ROOT_DIR}/Playground ${CMAKE_CURRENT_BINARY_DIR}/Playground)
add_subdirectory(${PROJECT_ROOT_DIR}/Showcase ${CMAKE_CURRENT_BINARY_DIR}/Showcase)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Playground)
