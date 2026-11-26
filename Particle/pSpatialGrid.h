/// PSpatialGrid.h
///
/// Copyright 1997-2007, 2022 by David K. McAllister
///
/// A spatial grid of a particle group's positions, used to find each particle's neighbors quickly for SPH().
/// Library users need not access this file.

#ifndef PSpatialGrid_h
#define PSpatialGrid_h

#include "Particle/pParticle.h"

#include <cmath>
#include <vector>

namespace PAPI {

/// Bins the particles of a group into cubic cells whose edge is the interaction radius h, so all particles within h of a point are in the
/// 27 cells around it. Cells are found through a hash table, so memory is proportional to the number of particles even when some particles
/// are very far from the others. Also holds each particle's SPH density when built by UpdateSpatialGrid() for inline SPH().
struct PSpatialGrid_t {
    float h = 0.f;     // Cell edge length and interaction radius
    float inv_h = 0.f; // 1 / h
    size_t count = 0;  // Number of particles binned
    bool ready = false; // Set by UpdateSpatialGrid(); cleared at the end of the next ParticleLoop()

    std::vector<float> density; // SPH density of each particle, in particle order

    void Clear()
    {
        count = 0;
        ready = false;
    }

    /// Bin the particles in [ibegin, iend) into cells of edge radius. Particle indices are relative to ibegin.
    void Build(const Particle_t* ibegin, const Particle_t* iend, const float radius)
    {
        h = radius;
        inv_h = 1.f / radius;
        count = size_t(iend - ibegin);
        ready = false;

        unsigned int tableSize = 64;
        while (tableSize < 2 * count) tableSize *= 2;
        mask = tableSize - 1;

        // Counting sort of the particle indices by hash bucket
        start.assign(size_t(tableSize) + 1, 0);
        bucketOf.resize(count);
        for (size_t i = 0; i < count; ++i) {
            const pVec& p = ibegin[i].pos;
            const unsigned int b = Bucket(CellCoord(p.x()), CellCoord(p.y()), CellCoord(p.z()));
            bucketOf[i] = b;
            start[size_t(b) + 1]++;
        }
        for (unsigned int b = 0; b < tableSize; ++b) start[size_t(b) + 1] += start[b];

        fill.assign(start.begin(), start.end() - 1);
        sorted.resize(count);
        for (size_t i = 0; i < count; ++i) sorted[fill[bucketOf[i]]++] = (unsigned int)i;
    }

    /// Call f(j) once for the index j of each particle in the 27 cells around p.
    /// That includes every particle within h of p, and some further away, so f must check the distance.
    template <class F> void ForEachNeighbor(const pVec& p, F&& f) const
    {
        if (count == 0) return;
        const int cx = CellCoord(p.x()), cy = CellCoord(p.y()), cz = CellCoord(p.z());
        unsigned int seen[27];
        int nSeen = 0;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const unsigned int b = Bucket(cx + dx, cy + dy, cz + dz);
                    bool dup = false; // Two of the cells can hash to the same bucket. Visit its particles only once.
                    for (int k = 0; k < nSeen; ++k) dup = dup || seen[k] == b;
                    if (dup) continue;
                    seen[nSeen++] = b;
                    for (unsigned int s = start[b]; s < start[size_t(b) + 1]; ++s) f(sorted[s]);
                }
    }

private:
    unsigned int mask = 0;              // Hash table size - 1. The size is a power of 2.
    std::vector<unsigned int> start;    // The particles in hash bucket b are sorted[start[b]] up to sorted[start[b + 1]]
    std::vector<unsigned int> sorted;   // Particle indices, grouped by hash bucket
    std::vector<unsigned int> bucketOf; // Scratch space for Build()
    std::vector<unsigned int> fill;     // Scratch space for Build()

    // Cell coordinate along one axis. Clamped so that huge or non-finite positions can't overflow the int.
    int CellCoord(const float x) const
    {
        float c = floorf(x * inv_h);
        if (!(c > -1e8f)) c = -1e8f; // Also catches NaN
        if (c > 1e8f) c = 1e8f;
        return int(c);
    }

    unsigned int Bucket(const int x, const int y, const int z) const
    {
        return ((unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ (unsigned int)z * 83492791u) & mask;
    }
};

} // namespace PAPI

#endif
