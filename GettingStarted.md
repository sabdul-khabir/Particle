# Getting Started with the Particle System API

This guide gives a high-level overview of the library's architecture, its core concepts, and how to write fast particle simulations with it.

The Particle System API is a C++ library for simulating large numbers of particles for interactive 3D graphics. Since version 3.0 it is built around **inline action mode**, in which your code owns the loop over particles and applies every action to a particle in a single pass. This runs at around 30 million particle updates per second on a laptop CPU.

## 1. Core Concepts

### Particle Context (`ParticleContext_t`)
The context is your main interface. It holds all the state for your simulations, including particle groups and action lists. Create one context per thread that runs particle systems.

### Particle Groups
Particles live in groups. Create groups with `GenParticleGroups()`, choose the one that actions apply to with `CurrentGroup()`, and set its capacity with `SetMaxParticles()`.

### Particles (`Particle_t`)
A particle is a struct with these attributes:
*   **Physics:** position, velocity, mass, age.
*   **Visuals:** color, alpha, size.
*   **Orientation:** up vector and rotational velocity.
*   **Secondary state:** `posB`, `upB` and `velB`, used by actions such as `Restore()` and `CopyVertexB()` and by renderers for motion blur or orientation.
*   **User data:** `data`, a 32-bit value you can set per particle and read in callbacks. (Note that the `Waypoints()` action uses it; see below.)

### Domains (`pDomain`)
Domains are regions of 3D space, such as `PDPoint`, `PDLine`, `PDSphere`, `PDBox`, `PDPlane`, `PDDisc` and `PDCylinder`. They are used to:
*   **Generate values:** a random point in the domain becomes a new particle's position, velocity, color, and so on.
*   **Test particles:** actions such as `Bounce()`, `Avoid()` and `Sink()` check whether particles are in, or about to cross, a domain. Not every action supports every domain; for example `Avoid()` supports only `PDSphere`, `PDRectangle`, `PDTriangle`, `PDDisc` and `PDPlane`. See each action's documentation.

### Source State (`pSourceState`)
A `pSourceState` is a template for new particles. Configure it with domains for velocity, color, size, and so on. When `Source()` or `Vertex()` creates a particle, each attribute is a random value drawn from its domain.

---

## 2. The Core Workflow

A simulation typically follows these steps:

1.  **Initialization:** create a `ParticleContext_t`, generate a group with `GenParticleGroups()`, and make it current with `CurrentGroup()`.
2.  **Time step:** each frame, set the time step `dt` with `TimeStep()`. Using the real frame time makes the simulation frame-rate independent.
3.  **Emission:** configure a `pSourceState` and call `Source()` or `Vertex()` to add particles.
4.  **Simulation:** call `ParticleLoop()` with a lambda that applies actions (`Gravity()`, `Bounce()`, `Move()`, and so on) to each particle.
5.  **Cleanup:** call `CommitKills()`. Inline `Sink()`, `SinkVelocity()` and `KillOld()` only *mark* particles for deletion; they stay in the group until `CommitKills()` runs.
6.  **Rendering:** get the particle data with `GetParticlePointer()` (no copy) or `GetParticles()` (copied into your arrays) and draw it with your graphics API.

---

## 3. Code Example: A Basic Fountain

This example uses inline mode, the recommended way to use the API.

```cpp
#include "Particle/pAPI.h" // Also brings in the inline action definitions
#include <execution>       // For parallel execution policies

using namespace PAPI;

ParticleContext_t P;

// Domains used inside the particle loop. Build them once rather than once per particle.
const PDPlane Floor(pVec(0, 0, 0), pVec(0, 0, 1));       // Positive side (z >= 0) is "inside"
const PDPlane KillPlane(pVec(0, 0, -5), pVec(0, 0, 1));

void Init()
{
    // Make a group that can hold up to 100,000 particles
    int group = P.GenParticleGroups(1, 100000);
    P.CurrentGroup(group);
}

void Update(float frameSeconds)
{
    P.TimeStep(frameSeconds); // Frame-rate independent: rates and accelerations are per second

    // Configure the template for new particles
    pSourceState S;
    S.Velocity(PDCylinder(pVec(0, 0, 5), pVec(0, 0, 7), 0.5f));
    S.Color(PDLine(pVec(0.5f, 0.5f, 1.0f), pVec(1, 1, 1))); // Bluish-white

    // Emit 2000 particles per second at the origin
    P.Source(2000, PDPoint(pVec(0, 0, 0)), S);

    // Apply all actions to each particle in one parallel pass
    P.ParticleLoop(std::execution::par_unseq, [&](Particle_t& p) {
        P.Gravity(p, pVec(0, 0, -9.8f));      // Accelerate downward
        P.Bounce(p, 0.1f, 0.5f, 0.1f, Floor); // Bounce off the floor (last velocity change before Move)
        P.Move(p);                            // Apply velocity to position
        P.Sink(p, false, KillPlane);          // Mark particles that fell below z = -5 (outside the plane's positive side)
    });

    // Actually remove the marked particles
    P.CommitKills();
}
```

---

## 4. Performance Best Practices

### Use Inline Mode
Prefer inline actions (`P.Gravity(p, ...)` inside `ParticleLoop()`) over legacy actions (`P.Gravity(...)` called on its own). Legacy mode makes a full pass over every particle for *every* action; inline mode makes **one** pass for **all** actions, keeping each particle in the CPU cache. Legacy mode is still needed for action lists and for the few actions that are legacy-only (`SPH()`, `DensityColor()`).

### Parallelize via Execution Policies
Pass `std::execution::par_unseq` to `ParticleLoop()` to spread the work across CPU cores and SIMD lanes. With GCC's standard library this requires linking against Intel TBB.

Actions that read *other* particles (`Follow()`, `Gravitate()`, `MatchVelocity()`, `MatchRotVelocity()`, `ResolveCollisions()`) see their neighbors part-way through being updated in inline mode. With a parallel policy the results vary from run to run. Use a sequential loop, or the legacy version of the action, when you need repeatable results.

### Build Domains Outside the Loop
A domain written inside the `ParticleLoop()` lambda, for example `P.Sink(p, false, PDPlane(...))`, is constructed again for every particle. Construct it once before the loop, as in the example above.

### Action Ordering
*   **Velocity, then position:** apply forces (gravity, jets, orbits) first, then constraints such as `Bounce()` and `ResolveCollisions()`, then `Move()`. `Bounce()` checks whether the particle *would* cross the domain during `Move()`, so it should be the last action that changes velocity before `Move()`.
*   **Avoid O(N²) actions on large groups:** `Gravitate()`, `MatchVelocity()`, `ResolveCollisions()`, `SPH()` and `DensityColor()` compare every particle with every other particle. Use them on small groups, or limit their reach with `max_radius` where available.

### Rendering Efficiency
Use `GetParticlePointer()` instead of `GetParticles()`. It returns a pointer to the API's own particle memory plus the offset of each attribute and the stride between particles, which you can pass directly to `glVertexAttribPointer()` or `glBufferData()` without a CPU-side copy.

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

## 7. Newer Actions

These actions were added after version 3.0. See their full documentation in `pActionDecls.h` and `pAPIContext.h`.

| Action | What it does | Notes |
|---|---|---|
| `Waypoints()` | Steers particles along a list of points, optionally looping | Stores each particle's current waypoint index in `Particle_t::data`, so don't use it on particles whose `data` holds your own values. Combine with `Damping()` or a generous `arrival_radius` so particles don't circle a waypoint. |
| `VoxelField()` | Adds accelerations sampled from a 3D grid of vectors | Field data is laid out x-fastest: `field[(z * dim_y + y) * dim_x + x]`. Trilinear interpolation by default. |
| `ResolveCollisions()` | Bounces particles off each other as spheres of radius `size.x` | Changes velocity only; it doesn't separate particles that already overlap. O(N²). |
| `SPH()` | Smoothed Particle Hydrodynamics fluid | Legacy API only. Stiff: lower `dt` or `pressure_multiplier` if the fluid explodes. O(N²). |
| `PositionColor()` | Maps position x/y/z to color r/g/b over a box | |
| `VelocityColor()`, `VelocitySize()` | Map speed to color or size between two values | |
| `RandomColor()` | Adds a random color change each step, like `RandomAccel()` for color | |
| `DensityColor()` | Colors particles by how many neighbors are within a radius | Legacy API only. O(N²). |

For example, a stream of particles that follows a racetrack and reddens as it speeds up:

```cpp
const pVec track[] = {pVec(10, 0, 0), pVec(0, 10, 0), pVec(-10, 0, 0), pVec(0, -10, 0)};

P.ParticleLoop(std::execution::par_unseq, [&](Particle_t& p) {
    P.Waypoints(p, track, 4, 5.0f, 2.0f);                        // magnitude 5, arrive within 2 units
    P.Damping(p, pVec(0.5f));                                    // Bleed off speed so particles don't overshoot
    P.VelocityColor(p, 0.0f, 8.0f, pVec(0, 0, 1), pVec(1, 0, 0)); // Blue when slow, red at 8 units/second
    P.Move(p);
});
```

---

## 8. Available Domains Summary

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

---

## 9. Where to Go Next

1.  **Run `Showcase`:** 17 short scenes that together use every action, domain and emission feature, each listing on screen exactly what it shows. Press C in the program for a table of which scene shows each feature, then read that scene's `Update()` in `Showcase/Showcase.cpp`.
2.  **Read `Example/Example.cpp`:** the smallest complete OpenGL program using the API.
3.  **Read `DemoShared/Effects.cpp`:** many complete effects, including flocking, used by the Playground and Benchmark apps.
4.  **Browse `Particle/pActionDecls.h`:** the documented list of all actions, shared by the inline and legacy APIs.
5.  **Check your domains:** many problems, such as particles leaking through walls, come from planes and discs whose normals point the wrong way. For `PDPlane`, the side the normal points to is "inside".
