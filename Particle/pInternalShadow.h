/// PInternalShadow.h
///
/// Copyright 1997-2007, 2022 by David K. McAllister
///
/// A shadow copy of some of the private library internals
/// Library users need not access this file.

#include <memory> // For std::shared_ptr

#ifndef PInternalShadow_h
#define PInternalShadow_h

#include <memory>

namespace PAPI {
struct Particle_t;
struct PSpatialGrid_t;

// Shadow copy of some information from PInternalState_t that is used by the inline actions API
// It is owned by pContextActions_t.
// It is updated by StartParticleLoop(), called from pContextActions_t::ParticleLoop().
class PInternalShadow_t {
public:
    float get_dt() { return dt; }
    const Particle_t* get_const_pgroup_begin() { return ibegin; } // Iterator to beginning of current particle group
    const Particle_t* get_const_pgroup_end() { return iend; }     // Iterator to end of current particle group
    Particle_t* get_pgroup_begin() { return ibegin; }             // Iterator to beginning of current particle group
    Particle_t* get_pgroup_end() { return iend; }                 // Iterator to end of current particle group
    bool get_in_new_list() const { return in_new_list; }
    bool get_in_particle_loop() const { return in_particle_loop; }
    const PSpatialGrid_t* get_sph_grid() const { return sph_grid; } // The current group's grid for inline SPH(), or null if not ready

    float dt;
    Particle_t* ibegin;
    Particle_t* iend;
    const PSpatialGrid_t* sph_grid = nullptr;

    bool in_new_list;
    bool in_particle_loop;
};

class PInternalState_t; // The API-internal struct containing the context's state. Don't try to use it.

void StartParticleLoop(std::shared_ptr<PInternalState_t> PS, PInternalShadow_t& PSh);
void EndParticleLoop(std::shared_ptr<PInternalState_t> PS, PInternalShadow_t& PSh);
} // namespace PAPI

#endif
