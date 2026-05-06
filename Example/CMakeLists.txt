# Example executable using Particle System API

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(EXE_NAME Example)

project(${EXE_NAME})

# set(CMAKE_CXX_STANDARD 17)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(GLEW REQUIRED)

include_directories(${GLEW_INCLUDE_DIRS})

# Actually search for TBB
find_package(TBB)

# Find GLUT
find_package(GLUT REQUIRED)
include_directories(${GLUT_INCLUDE_DIR})

set(SOURCES Example.cpp)

# Use standard defines for static linking if needed
add_definitions(-DGLEW_STATIC -DFREEGLUT_STATIC)

source_group("src"  FILES ${SOURCES})

add_executable(${EXE_NAME} ${SOURCES})

set_target_properties(${EXE_NAME} PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_ROOT_DIR}
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(${EXE_NAME} PRIVATE Particle GLEW::GLEW ${GLUT_LIBRARIES})

if(TBB_FOUND)
    target_link_libraries(${EXE_NAME} PRIVATE TBB::tbb)
endif()