![Awesome particle simulation](DemoShared/Screenshot_5.jpg)

# The Particle System API

*Since 1999, using C++, OpenGL, GLUT, and CMake*

by David McAllister
version 3.0.0
April 18, 2022

Hosted at https://github.com/davemc0/Particle.git.
(Previously hosted for 15 years at: http://www.particlesystems.org.)

New to the API? Start with the [Getting Started guide](./GettingStarted.md).

Documentation is [here](./ParticleLib/doc/html/index.html).

Change log is [here](./ChangeLog.html).

Future work is [here](./ToDo.html).

Welcome back to the Particle System API!
==================================

The Particle System API has served as a resource for building all sorts of interesting particle system-based special effects, mostly for games and other interactive 3D graphics.
Version 2.2 was used in projects by over 600 developers. I dropped maintenance of the library in 2009 when my life switched to ray tracing, but I didn't effectively host the library publicly, so it largely died out.

I have recently revisited the API and added many enhancements. The most important is the _inline action mode_. The actions are now implemented in inline functions that apply to one particle. The application owns the loop over the particles, allowing a single pass over a particle to perform all actions. Performance is massively faster because of this for three main reasons:
1) The compiler can optimize computations across all the actions on a particle,
2) The particle is only loaded into memory once per time step, and
3) The particle computations are trivially parallelized across cores, and potentially SIMD lanes by using std::for_each with a parallel execution policy on C++17.
Performance is now at around 30 million particle updates per second on my laptop with a moderate CPU. And the inline actions should be usable in CUDA kernels for a major GPU speedup.

A few other notable updates:
* The build system is now based on CMake.
* The project is hosted on GitHub. Your collaboration is invited.
* The sample effects are much more elegant and unified and are frame rate independent by varying dt with frame time.
* There is a good flocking behavior effect, based on Craig Reynolds' Boids.

Building the Program
====================

To build everything you need:

* CMake 3.20 or newer, and a C++17 compiler. Visual Studio 2022 works; it also includes a copy of CMake.
* oneTBB (Intel's Threading Building Blocks), for example from the Intel oneAPI installer. **The library itself needs it**:
  the inline API's headers include `ParticleLib/PInternalState.h`, which uses `tbb::concurrent_vector`.
  Linking to the `Particle` CMake target adds TBB's include folder and library for you.
* FreeGLUT, built as a static library (`freeglut_static.lib`). I use 3.2.2.
* GLEW. I use 2.2.0. On Windows the demos link the static `glew32s.lib`.

FreeGLUT, GLEW and OpenGL are only used by Example, Playground and Showcase; ParticleLib and Benchmark need only TBB.
However, the top-level CMakeLists.txt looks for all of them, so you need all of them to generate the solution.
The demos no longer depend on DMcTools. Playground loads images with the copy of stb_image in `third_party/stb`.

The library is compiled with AVX2 instructions (`/arch:AVX2`, or `-mavx2` with GCC and Clang), so it needs a CPU that supports AVX2.

Telling CMake where the dependencies are
----------------------------------------

On Windows, set these, either as environment variables or with `-D` on the CMake command line:

* `GLEW_HOME`: the GLEW folder. It contains `include/GL/glew.h` and `lib/Release/x64/glew32s.lib`, as in the prebuilt Windows download.
* `GLUT_HOME`: the FreeGLUT folder. It contains `include/GL/freeglut.h`, `lib/Release/freeglut_static.lib` and
  `lib/Debug/freeglut_staticd.lib`, as you get by building FreeGLUT with CMake in its own folder.
* `TBB_DIR`: only needed if CMake doesn't find TBB by itself. It is the folder containing `TBBConfig.cmake`,
  such as `C:/Program Files (x86)/Intel/oneAPI/tbb/latest/lib/cmake/tbb`.

Instead of `GLEW_HOME` and `GLUT_HOME` you can set CMake's own cache variables for the exact files: `GLEW_INCLUDE_DIR`,
`GLEW_STATIC_LIBRARY_RELEASE`, `GLUT_INCLUDE_DIR`, `GLUT_glut_LIBRARY_RELEASE` and `GLUT_glut_LIBRARY_DEBUG`.

On Linux or Mac, installing the packages where CMake looks for them should be enough.

How to build
------------

In the main directory, do something like this, with your own folders:

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DGLEW_HOME=C:/libs/glew-2.2.0 -DGLUT_HOME=C:/libs/freeglut-3.2.2

Then you can build from the command line:

    cmake --build build --config Release

or open build/ParticleSolution.sln in Visual Studio. Playground is the startup project.

After building each program, the build copies the TBB DLL (`tbb12.dll`, or `tbb12_debug.dll` for Debug) into the program's folder,
so the programs run without TBB on your PATH. When you start a program from Visual Studio, its working directory is its
source folder, such as `Playground/`.

Example Code
============

Example
-------
An OpenGL and GLUT-based example of particle systems used to render a fountain

Showcase
--------
A guided tour of the whole API in 17 scenes. Together they use every action, every domain, and every way of
creating particles, and each scene lists on screen exactly which actions, domains, and emission features it is showing.
It is the best place to see what a feature does before reading its code: each scene's simulation is in one short
`Update()` function in [Showcase/Showcase.cpp](Showcase/Showcase.cpp). Like Example, it needs FreeGLUT and GLEW.

* Left/Right arrows (or N/P) change scenes; 1-9 jump to a scene; A starts an automatic tour.
* Space does something specific to the scene (explode, refill, toggle an option); the bottom line says what.
* C shows a table of every feature and the scenes that demonstrate it.
* Drag to orbit the camera and use the wheel to zoom.
* `Showcase --selftest` runs every scene without a window and checks that the particles behave sensibly.
* `Showcase --shots <dir>` saves a screenshot of every scene.

Playground
----------
Double-click to start the program. Playground loads `sample.jpg` from its working directory for its photo effect, so start it
from the `Playground/` folder, or from Visual Studio. Without the image, the photo effect shows a generated test pattern instead.

Press 'f' to make it full screen.

Right-click in the window to pop up the menu.
The keyboard shortcuts are listed on the menu.

If you don't touch it, it will randomly go through the demos. This should provide hours of family fun.
If you get bored, press 'space' to cause an explosion.
Press 'm' to make cool motion blur.
Press 's' to sort the particles back-to-front for more correct but slower rendering.
Press 'p' to change what each particle is rendered as.

![Awesome particle simulation](DemoShared/Screenshot_7.jpg)

Benchmark
---------
This one runs all the same demo effects as Playground does,
but it doesn't do any graphics. It doesn't use OpenGL or GLUT.

Final Notes
===========

If the demos don't work for you, I'd be happy to try to help. Please include as much detail as
you can about your system and the nature of the problem.

If you have any questions, code contributions or suggestions use github issues or contact me however.

![Awesome particle simulation](DemoShared/Screenshot_2.jpg)
