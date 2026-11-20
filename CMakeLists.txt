# Particle System API Main Solution

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(EXE_NAME ParticleSolution)

project(${EXE_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
    # build in parallel (MSVC-specific)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/MP>)
else()
    # If building in Debug mode on MinGW, libraries often don't have a 'd' suffix.
    # This helps find_package(GLEW) and others succeed.
    set(CMAKE_DEBUG_POSTFIX "")
endif()

##################################################################
# Third-party dependencies. README.md ("Building the Program") explains how to point CMake at them.

# oneTBB is required by the library itself: the inline API includes ParticleLib/PInternalState.h, which uses tbb::concurrent_vector.
# If CMake doesn't find it, set TBB_DIR to the folder that contains TBBConfig.cmake.
find_package(TBB REQUIRED)

# GLEW, FreeGLUT and OpenGL are used by Example, Playground and Showcase. Benchmark and the library don't need them.
set(GLEW_HOME "$ENV{GLEW_HOME}" CACHE PATH "GLEW folder, containing include/GL/glew.h and lib/Release/x64/glew32s.lib")
set(GLUT_HOME "$ENV{GLUT_HOME}" CACHE PATH "FreeGLUT folder, containing include/GL/freeglut.h and lib/Release/freeglut_static.lib")

# On Windows, link GLEW statically so the programs don't need glew32.dll.
if(GLEW_HOME)
    list(APPEND CMAKE_PREFIX_PATH "${GLEW_HOME}")
endif()
if(WIN32)
    set(GLEW_USE_STATIC_LIBS TRUE)
endif()
find_package(GLEW REQUIRED)
if(GLEW_USE_STATIC_LIBS)
    # FindGLEW doesn't add this define itself. Without it the GLEW header expects glew32.dll.
    set_property(TARGET GLEW::GLEW APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS GLEW_STATIC)
endif()

# The demos link FreeGLUT statically. CMake's FindGLUT doesn't search for the static library's names, so find them here first.
if(GLUT_HOME)
    find_path(GLUT_INCLUDE_DIR GL/freeglut.h PATHS "${GLUT_HOME}/include" NO_DEFAULT_PATH)
    find_library(GLUT_glut_LIBRARY_RELEASE NAMES freeglut_static PATHS "${GLUT_HOME}/lib/Release" "${GLUT_HOME}/lib" NO_DEFAULT_PATH)
    find_library(GLUT_glut_LIBRARY_DEBUG NAMES freeglut_staticd PATHS "${GLUT_HOME}/lib/Debug" "${GLUT_HOME}/lib" NO_DEFAULT_PATH)
endif()
find_package(GLUT REQUIRED)
set_property(TARGET GLUT::GLUT APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS FREEGLUT_STATIC)

find_package(OpenGL REQUIRED)

# Copy the TBB DLL next to a program after building it, so the program runs from Visual Studio or Explorer without TBB on the PATH.
function(particle_copy_runtime_dlls target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:TBB::tbb>" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()

##################################################################

add_subdirectory(${PROJECT_ROOT_DIR}/ParticleLib ${CMAKE_CURRENT_BINARY_DIR}/ParticleLib)

add_subdirectory(${PROJECT_ROOT_DIR}/Benchmark ${CMAKE_CURRENT_BINARY_DIR}/Benchmark)
add_subdirectory(${PROJECT_ROOT_DIR}/Example ${CMAKE_CURRENT_BINARY_DIR}/Example)
add_subdirectory(${PROJECT_ROOT_DIR}/Playground ${CMAKE_CURRENT_BINARY_DIR}/Playground)
add_subdirectory(${PROJECT_ROOT_DIR}/Showcase ${CMAKE_CURRENT_BINARY_DIR}/Showcase)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Playground)
