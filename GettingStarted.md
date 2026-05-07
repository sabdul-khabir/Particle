# Getting Started with the Particle System API

Welcome to the Particle System API! This guide provides a high-level overview of the library’s architecture, its core systems, and how to write high-performance particle simulations.

The Particle System API is a performant C++ library designed for simulating large numbers of particles for interactive 3D graphics. Since its overhaul in version 3.0, it focuses heavily on **Inline Action Mode**, which leverages modern C++ features for massive performance gains (up to 30 million updates per second).

## 1. Core Concepts

### Particle Context (`ParticleContext_t`)
The context is your main interface. It holds all the state for your simulations, including particle groups and action lists. Typically, you create one context per thread.

### Particles (`Particle_t`)
A particle is a struct containing physical and visual attributes:
*   **Physics:** Position, Velocity, Mass, Age.
*   **Visuals:** Color, Alpha, Size.
*   **Orientation:** Up vector and Rotational Velocity.
*   **Secondary State:** `PositionB` and `VelocityB` (often used for target-seeking or trail effects).

### Domains (`pDomain`)
Domains represent regions of 3D space. They are used for:
*   **Generation:** Where particles are born (e.g., `PDSphere`, `PDBox`).
*   **Logic:** Where particles bounce (`PDPlane`), avoid obstacles (`PDCone`), or are destroyed (`PDDisc`).

### Source State (`pSourceState`)
Think of this as a template or "blueprint" for new particles. Before you emit particles, you configure a `pSourceState` with domains for velocity, color, size, etc. When `Source()` is called, it pulls random values from these domains to initialize each particle.

---

## 2. The Core Workflow

Implementing a simulation typically follows these steps:

1.  **Initialization:** Create a `ParticleContext_t` and generate a group handle via `GenParticleGroups`.
2.  **Configuration:** Set the `TimeStep` (dt).
3.  **Emission:** Define a `pSourceState` and call `Source()` to add particles.
4.  **Simulation Loop:** Use `ParticleLoop` to apply actions (Gravity, Bounce, Move, etc.).
5.  **Clean-up:** Call `CommitKills()` to remove particles tagged for deletion from memory.
6.  **Rendering:** Retrieve particle pointers via `GetParticlePointer()` to draw via OpenGL/DirectX.

---

## 3. Code Example: A Basic Fountain

This example demonstrates the modern **Inline Mode**, which is the recommended way to use the API for maximum performance.

```cpp
#include "Particle/pAPI.h"
#include <execution> // For parallel execution policies

using namespace PAPI;

ParticleContext_t P;
int groupHandle;

void Init() {
    // 1. Generate a group that can hold up to 100k particles
    groupHandle = P.GenParticleGroups(1, 100000);
    P.CurrentGroup(groupHandle);
    P.TimeStep(0.1f);
}

void Update(float frameTime) {
    // 2. Configure the birth template
    pSourceState S;
    S.Velocity(PDCylinder(pVec(0, 0, 0.5f), pVec(0, 0, 0.7f), 0.05f));
    S.Color(PDLine(pVec(0.5, 0.5, 1.0), pVec(1, 1, 1))); // Bluish-white

    // 3. Emit 200 particles per unit time at the origin
    P.Source(200, PDPoint(pVec(0, 0, 0)), S);

    // 4. Run the simulation loop (Parallelized via C++17)
    P.ParticleLoop(std::execution::par_unseq, [&](Particle_t& p) {
        P.Gravity(p, pVec(0, 0, -0.01f)); // Pull down
        P.Bounce(p, 0.1f, 0.5f, 0.01f, PDPlane(pVec(0,0,0), pVec(0,0,1))); // Bounce on floor
        P.Sink(p, false, PDPlane(pVec(0,0,-5), pVec(0,0,1))); // Kill if they fall too far
        P.Move(p); // Integrate velocity into position
    });

    // 5. Actually remove the dead particles from memory
    P.CommitKills();
}
```

---

## 4. Performance Best Practices

### Use Inline Mode
Avoid the "Legacy Mode" (calling actions like `P.Gravity()` without a `Particle_t` reference). Legacy mode performs a full memory pass for *every* action. Inline mode performs **one** pass for **all** actions, drastically reducing cache misses.

### Parallelize via Execution Policies
The `ParticleLoop` is designed to be "embarrassingly parallel." Always use `std::execution::par_unseq` if your compiler supports C++17 to distribute the load across all CPU cores and enable SIMD optimizations.

### Action Ordering
*   **Move at the end:** Always call `Move()` as the last action in your loop. This ensures that all forces (Gravity) and constraints (Bounce) calculated in the current step are applied to the final position.
*   **Avoid $O(N^2)$ actions:** Actions like `Gravitate()` or `MatchVelocity()` check every particle against every other particle. Use them sparingly or limit their influence with a small `max_radius`.

### Secondary Emission (Inline Source/Vertex)

A unique feature of Inline Mode is the ability to emit new particles from *existing* particles. This is ideal for trails, sparks, or debris. When `Source()` or `Vertex()` are called inside a `ParticleLoop`, the first argument is the current particle (`p`), and the position is treated as an offset relative to that particle.

Because the loop is parallel, these new particles are not added to memory immediately. You must call `CommitSources()` after the loop to finalize their creation.

```cpp
void Update(float dt) {
    pSourceState S_Trail;
    S_Trail.Color(PDPoint(pVec(1, 0.4, 0))); // Orange trail
    S_Trail.StartingAge(0, 0.1f);

    P.ParticleLoop(std::execution::par_unseq, [&](Particle_t& p) {
        // 1. Standard physics
        P.Gravity(p, pVec(0, 0, -0.05f));
        
        // 2. Inline Source: Each existing particle spawns 5 trail particles per unit time.
        // The PDPoint(pVec(0)) means they are born exactly at the parent's current position.
        P.Source(p, 5.0f, PDPoint(pVec(0, 0, 0)), S_Trail);

        // 3. Inline Vertex: Spawn one specific "spark" at a relative offset.
        if(p.age > 2.0f) {
            P.Vertex(p, pVec(0, 0.1f, 0), S_Trail);
        }

        P.Move(p);
    });

    // 4. Finalize the emission and cleanup
    P.CommitSources(); // Adds buffered trail particles to the group
    P.CommitKills();   // Removes dead particles
}
```

### Rendering Efficiency
Use `GetParticlePointer()` instead of `GetParticles()`. The pointer allows you to pass internal API memory directly to your graphics driver (via `glVertexPointer` or `glBufferData`), avoiding an expensive CPU-side memory copy.

---

## 5. Where to Start Implementing

1.  **Study `Example/Example.cpp`:** This is the baseline implementation for an OpenGL application.
2.  **Browse `pActionDecls.h`:** This file serves as the master list of all physical behaviors (Vortex, Orbit, Damping, etc.) available to you.
3.  **Define your Domains:** Most simulation issues (like particles "leaking" through walls) are caused by incorrectly oriented normals in domains like `PDPlane` or `PDDisc`.

---

## 6. Available Actions Summary

Below is a categorized list of actions available in the Particle System API.

### Emission & Lifecycle
*   `Source` / `Vertex`: Create new particles from domains or specific points.
*   `KillOld`: Remove particles based on their age.
*   `Sink` / `SinkVelocity`: Destroy particles that enter a spatial domain or exceed speed limits.
*   `CommitSources` / `CommitKills`: Buffering commands required to finalize births and deaths in Inline Mode.

### Basic Physics & Dynamics
*   `Gravity`: Constant acceleration vector (e.g., Earth gravity).
*   `Damping` / `RotDamping`: Linear and rotational friction to simulate air or water resistance.
*   `Move`: The primary integrator; applies velocity to position and rotational velocity to orientation.
*   `Explosion`: Spherical shockwave that pushes particles outward.
*   `Jet`: Acceleration applied to particles within a specific domain.
*   `SpeedClamp`: Keeps particle speeds within a defined range.

### Movement & Steering
*   `Avoid`: Steers particles to prevent them from entering a domain.
*   `Bounce`: Reflects particles off surfaces (planes, discs, spheres, etc.) with friction and resilience.
*   `OrbitPoint` / `OrbitLine`: Attracts particles toward a point or an axis.
*   `Waypoints`: Moves particles through a sequence of target positions.
*   `Vortex`: Tornado-like movement around an axis.
*   `RandomAccel` / `RandomDisplace`: Adds stochastic noise to movement.

### Inter-Particle Interactions
*   `MatchVelocity` / `MatchRotVelocity`: Flocking behavior (Alignment); particles mimic their neighbors.
*   `Gravitate`: N-Body simulation where every particle attracts every other particle.
*   `ResolveCollisions`: Elastic collisions between particles using their mass and radius.
*   `SPH`: Smoothed Particle Hydrodynamics for high-performance fluid and gas simulation.

### Attribute Management
*   `TargetColor` / `TargetSize` / `TargetVelocity`: Asymptotically transition attributes toward a target.
*   `DensityColor` / `VelocitySize`: Map physical properties to visual ones (e.g., color by local density).
*   `Callback`: Execute custom logic per-particle via a user-defined function.

---

## 7. Available Domains Summary

Domains represent spatial regions and are used for emission volumes, collision surfaces, and logic boundaries.

### Basic Primitives
*   `PDPoint`: A single 3D point.
*   `PDLine`: A line segment between two endpoints.

### Planar Domains
*   `PDTriangle`: A planar triangle defined by three vertices.
*   `PDRectangle`: A rhombus-shaped planar region defined by an origin and two basis vectors.
*   `PDDisc`: A circular disc or flat washer (annulus) with an inner and outer radius.
*   `PDPlane`: An infinite plane; often used for ground surfaces or "kill zones."

### Volumetric Domains
*   `PDBox`: An axis-aligned bounding box (AABB).
*   `PDSphere`: A solid sphere or a spherical shell.
*   `PDCylinder`: A solid cylinder or a cylindrical shell (tube).
*   `PDCone`: A solid cone or a conical shell.

### Special Domains
*   `PDBlob`: A radially symmetrical Gaussian probability density; useful for natural, "fuzzy" boundaries.
*   `PDUnion`: A CSG union of multiple sub-domains; automatically handles probability distribution based on the relative volume or area of its members.
```
