# Add a Bullet-powered 3D driving simulator

## Goal

Add the user's `mls-m5/bullet3-src` repository as a Git submodule and use it for the interactive vehicle dynamics. Add a custom OpenGL ES 2/WebGL 1 renderer for a procedural 3D road environment.

Keep three selectable modes:

- Automatic dummy telemetry.
- Interactive Bullet physics with retained 2D road view.
- Interactive Bullet physics with 3D road view.

The 3D view is the default for native interactive mode and Emscripten. Native flags are `--backend=dummy`, `--backend=simulator --view=3d`, and `--backend=simulator --view=2d`. Browser navigation exposes both simulator views and defaults to 3D.

## Requirements

- Build Bullet's collision, dynamics, and linear-math source from the pinned submodule without its demos or desktop OpenGL code.
- Keep physics independent of SDL, NanoVG, and rendering.
- Base battery use and regeneration on forces and motion calculated by Bullet, including traction work, drag, rolling resistance, braking, and drivetrain efficiency.
- Render using custom GLES2 shaders, vertex buffers, depth testing, and procedural geometry. Do not use fixed-function or desktop-only OpenGL.
- Include a road, lane markings, terrain, pine trees, mountains, a vehicle, steering movement, daylight variation, and headlights.
- Keep dashboard telemetry behind `VehicleDataSource`; a future Ethernet implementation remains possible without GUI changes.
- Preserve and test native and containerized Emscripten builds.

## Validation

Run clang-format, native build/CTest, launch 2D and 3D simulator modes, build the containerized Emscripten output, and run `git diff --check`. Move this file to `tasks/done/` when complete.
