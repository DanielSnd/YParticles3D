# YParticles3D

YParticles3D is a Shuriken-inspired CPU GDExtension Particle system for Godot 4 

## What It Does

YParticles3D is a `Node3D` you can drop into a scene and it will work as a full particle emitter.

- Lifetime, speed, size, rotation, gravity, looping, delays, playback speed, fixed FPS simulation
- Emission by rate over time, rate over distance, and timed bursts
- Shape-based emission including cone, sphere, hemisphere, box, circle, edge, and mesh
- Curves and gradients for size, velocity, force, color, alpha, rotation, trail width, and more
- Velocity over lifetime, force over lifetime, limit velocity, inherit velocity, and orbit-style motion
- Noise-driven motion using `FastNoiseLite`
- Attraction toward a position or another `Node3D`
- Particle collision settings with bounce, dampening, lifetime loss, and quality options
- Texture sheet animation
- Trails with separate lifetime, color, width, texture, and world/local space options
- Sub-emitters for birth, collision, and death events
- Rendering controls like blend mode, billboarding, alignment, render priority, layers, custom mesh, and custom material

## Editor Support

YParticles3D has:

- A custom inspector layout
- Grouped controls instead of one giant property dump
- Custom editors for bursts and sub-emitters

## Godot Version

The addon currently targets Godot `4.5+`.

You can see that in [`yparticles3d.gdextension`](/home/danielsnd/YParticles3D/test_project/addons/yparticles3d/yparticles3d.gdextension:3), which sets `compatibility_minimum = "4.5"`.

## Project Layout

- [`src`](/home/danielsnd/YParticles3D/src): C++ source for the particle node and editor integration
- [`test_project`](/home/danielsnd/YParticles3D/test_project): Godot project used for testing the addon
- [`test_project/addons/yparticles3d`](/home/danielsnd/YParticles3D/test_project/addons/yparticles3d): the actual addon folder that gets packaged
- [`tools`](/home/danielsnd/YParticles3D/tools): helper scripts for local builds and maintenance
- [`.github/workflows/build-plugin.yml`](/home/danielsnd/YParticles3D/.github/workflows/build-plugin.yml): cross-platform build and packaging workflow

## Trying It Locally

The quickest way to test it is to open [`test_project`](/home/danielsnd/YParticles3D/test_project) in Godot.

For a local rebuild:

1. Make sure `scons` is installed and available in your shell.
2. Build from the repo root:

```bash
scons target=editor compiledb=yes
```

That installs the local editor/debug binary into the addon folder used by `test_project`.

If you just want to use the helper script instead:

```bash
python tools/compile_debug_build.py
```

## Building Release Packages

The GitHub Actions workflow is set up to produce a zip that works in both places you care about:

- In the Godot editor
- In exported games

The important bit is the target split:

- `editor` builds are used for the addon's debug libraries, so editor-only tooling is included
- `template_release` builds are used for exported projects

The workflow also packages the full addon directory, not just the binaries, so the final zip includes:

- `yparticles3d.gdextension`
- shaders
- icons
- platform binaries

### Workflow Modes

- `debug`: builds editor-capable debug binaries for testing
- `full_plugin_compilation`: builds editor debug binaries plus release binaries for export

When the workflow finishes, it uploads a `finished_unzip_me` artifact containing the packaged plugin zip.

## Installing the Built Addon

Drop the packaged `yparticles3d` folder into your project's `addons` directory:

```text
addons/yparticles3d
```

Then open the project in Godot and enable the addon if needed.

## Building From Source

You need:

- Python
- SCons
- A working C++ toolchain
- Git submodules initialized

Clone the repo with submodules or run:

```bash
git submodule update --init --recursive
```

Then build with SCons from the project root. A typical local editor build is:

```bash
scons target=editor platform=linux arch=x86_64 precision=single
```

Adjust `platform`, `arch`, and `precision` as needed.

## Notes

- The local test project is part of the repo on purpose. It is the easiest place to verify runtime behavior and editor integration together.
- The addon includes a lot of curve- and gradient-driven behavior, so it is easiest to understand by opening it in the editor and poking around the inspector.
