# Particle (the actual library)

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(Particle)

set(LIB_SOURCES
    ActionExecute.cpp
    ActionStructs.h
    ActionsAPI.cpp
    LibHelpers.h
    OtherAPI.cpp
    PInternalState.h
    PInternalState.cpp
    ParticleGroup.h
)

set(API_SOURCES
    ../Particle/pAPI.h
    ../Particle/pAPIContext.h
    ../Particle/pActionDecls.h
    ../Particle/pActionImpls.h
    ../Particle/pDeclarations.h
    ../Particle/pDomain.h
    ../Particle/pError.h
    ../Particle/pInlineActionsAPI.h
    ../Particle/pInternalShadow.h
    ../Particle/pParticle.h
    ../Particle/pSourceState.h
    ../Particle/pSpatialGrid.h
    ../Particle/pVec.h
)

set(SOURCES
    ${LIB_SOURCES}
    ${API_SOURCES}
)

source_group("Lib" FILES ${LIB_SOURCES})
source_group("API" FILES ${API_SOURCES})

add_library(Particle STATIC ${SOURCES})

# TBB is part of the library's public interface: pInlineActionsAPI.h includes PInternalState.h, which uses tbb::concurrent_vector.
# The top-level CMakeLists.txt normally finds it; this covers building ParticleLib on its own.
if(NOT TARGET TBB::tbb)
    find_package(TBB REQUIRED)
endif()
target_link_libraries(Particle PUBLIC TBB::tbb)

# Push good flags out to library users
# Warning level, all warnings as errors, optimization
if(MSVC)
    target_compile_options(Particle PUBLIC /W3 /WX /fp:fast /Ot /Oi /Oy /GL /Gy /GF /Qpar /arch:AVX2)
    target_link_options(Particle INTERFACE /LTCG)
else()
    # Flags for GCC / Clang
    target_compile_options(Particle PUBLIC -Wall -Wextra -ffast-math -O3 -flto -ffunction-sections -fdata-sections -mavx2)
    target_link_options(Particle INTERFACE -flto)
endif()

target_include_directories(Particle
    PRIVATE "."
    PRIVATE ".."
)

# Tell users of the library what they need to include to use it
target_include_directories(Particle
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..
)
