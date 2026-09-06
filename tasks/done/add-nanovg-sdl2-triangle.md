# Add an SDL2 + NanoVG triangle example

## Goal

Turn the existing `car-gui` executable into a minimal graphical application that opens an SDL2 window and draws a triangle with NanoVG.

SDL2 must come from the system installation for native builds. NanoVG must be downloaded and integrated by CMake. The design should leave a straightforward path to compiling the same application with Emscripten/WebAssembly later.

## Graphics backend decision

Use NanoVG's **OpenGL ES 2** backend (`NANOVG_GLES2`) rather than its desktop OpenGL 2/3 backends.

Reasons:

- Emscripten WebGL 1 exposes an API based on OpenGL ES 2.0.
- NanoVG already has a GLES2 renderer, so the same NanoVG backend can be retained for native and future browser builds.
- It avoids adding GLEW, which NanoVG's desktop OpenGL backend normally expects and which is not currently installed.
- SDL2 can create an OpenGL ES 2 context on native platforms and a WebGL-compatible context under Emscripten.

The initial implementation should target GLES2/WebGL 1 as the portable baseline. Do not use desktop-only OpenGL calls, fixed-function rendering, or APIs newer than GLES2. A future move to GLES3/WebGL 2 can be considered separately if a feature requires it.

## Current project state

- `CMakeLists.txt` creates the `car-gui` C++17 executable.
- `src/main.cpp` only prints `hello there`.
- `lib/CMakeLists.txt` is empty and can hold the NanoVG dependency target.
- SDL2 2.30 is available through the system CMake package.
- System GLES2 headers and libraries are available.
- GLEW is not installed.

## Required changes

### 1. Integrate NanoVG in `lib/CMakeLists.txt`

Use CMake `FetchContent` to retrieve NanoVG from:

```text
https://github.com/memononen/nanovg.git
```

Pin `GIT_TAG` to a specific upstream commit so builds remain reproducible. Do not follow `master` implicitly.

NanoVG does not provide a CMake target itself. After populating the dependency, create a static library target from:

```text
${nanovg_SOURCE_DIR}/src/nanovg.c
```

The target should:

- Be named `nanovg` or namespaced with a local alias such as `NanoVG::NanoVG`.
- Publish `${nanovg_SOURCE_DIR}/src` as a public include directory.
- Compile as C; do not copy NanoVG source files into this repository.
- Avoid modifying files in the downloaded source tree.

Only one translation unit may define `NANOVG_GLES2_IMPLEMENTATION`. Prefer adding a small project-owned implementation source, for example `src/nanovg_gles2.cpp`, containing the implementation macro and inclusion of `nanovg_gl.h`. Alternatively it can be defined in `main.cpp`, but a separate source is preferable because it keeps third-party implementation details out of application code.

The implementation translation unit should effectively contain:

```cpp
#define NANOVG_GLES2_IMPLEMENTATION
#include <nanovg_gl.h>
```

Application code that needs the backend declarations must include `nanovg_gl.h` with `NANOVG_GLES2` selected, but must not define the implementation macro a second time. This may be handled with a small project wrapper header or an appropriate target compile definition.

### 2. Update the root `CMakeLists.txt`

Keep C++17 and continue adding `lib` before linking the executable.

For a normal native build:

- Find SDL2 from the system with `find_package(SDL2 REQUIRED CONFIG)`.
- Find the system GLES2 implementation using CMake's `FindOpenGL` support.
- Link `car-gui` to `SDL2::SDL2`, the local NanoVG target, and `OpenGL::GLES2`.
- Remove the unused `Threads` dependency.

If `OpenGL::GLES2` requires a newer `FindOpenGL` module than the project's current CMake minimum, either raise the minimum to the first supported version and document it, or implement a small, clearly scoped fallback. Prefer imported CMake targets over raw library variables.

Do not fetch SDL2 for native builds.

Structure the CMake code so an Emscripten branch can be introduced without reorganizing all targets. In a future Emscripten build:

- SDL2 should be supplied by Emscripten using `-sUSE_SDL=2` rather than the host system package.
- GLES2/WebGL libraries should be supplied by the Emscripten toolchain rather than found on the host.
- NanoVG should continue to use the same GLES2 backend.
- The executable will need suitable Emscripten link options and an `.html` output or shell page.

The current task does not need to produce a WebAssembly build, but it must not make that future branch unnecessarily difficult.

### 3. Replace `src/main.cpp` with the graphical example

Implement a minimal SDL/NanoVG application.

#### Initialization

1. Initialize `SDL_INIT_VIDEO`.
2. Set SDL OpenGL attributes before creating the window:
   - `SDL_GL_CONTEXT_PROFILE_MASK` to `SDL_GL_CONTEXT_PROFILE_ES`.
   - Major version 2 and minor version 0.
   - Double buffering enabled.
   - At least 8 stencil bits, since NanoVG uses the stencil buffer.
   - A sensible color buffer configuration if needed.
3. Create a window with:
   - `SDL_WINDOW_OPENGL`.
   - `SDL_WINDOW_RESIZABLE`.
   - `SDL_WINDOW_ALLOW_HIGHDPI` where supported.
4. Create and activate the SDL GL context.
5. Optionally enable vertical synchronization with `SDL_GL_SetSwapInterval(1)`. Failure to enable vsync should be non-fatal.
6. Create the NanoVG context with `nvgCreateGLES2`, using at least `NVG_ANTIALIAS | NVG_STENCIL_STROKES`.

Every failure must print a useful diagnostic and clean up resources that were already created.

#### Application state and frame function

Keep the state needed by one frame in a small application structure, such as:

- `SDL_Window*`
- `SDL_GLContext`
- `NVGcontext*`
- A running flag

Put event processing and rendering into callable functions rather than embedding everything in an unconditional infinite loop. A suggested shape is:

```cpp
struct Application;
bool initialize(Application& app);
void frame(Application& app);
void shutdown(Application& app);
```

This is important for future Emscripten support. Native builds can call `frame` from a `while (running)` loop. A future browser build can pass the same state/frame function to `emscripten_set_main_loop_arg`, because browser applications must return control to the browser between frames.

Do not add Asyncify or rely on blocking the browser event loop.

#### Per-frame behavior

Each frame should:

1. Poll SDL events.
2. Exit when receiving `SDL_QUIT` or an Escape key press.
3. Obtain both the logical SDL window size and the drawable framebuffer size.
4. Calculate the device-pixel ratio as drawable width divided by logical width, guarding against zero-sized/minimized windows.
5. Set `glViewport` using drawable pixel dimensions.
6. Clear the color and stencil buffers.
7. Call `nvgBeginFrame` with logical dimensions and the calculated pixel ratio.
8. Build a centered triangle with `nvgBeginPath`, `nvgMoveTo`, `nvgLineTo`, and `nvgClosePath`.
9. Fill it with a clearly visible NanoVG color. A contrasting stroke is optional.
10. Call `nvgEndFrame`.
11. Present the frame with `SDL_GL_SwapWindow`.

The triangle should remain centered and scale reasonably when the window is resized. Use the current window dimensions rather than hard-coded absolute coordinates for all three vertices.

Only GLES2-compatible GL calls should be used. SDL and NanoVG should handle the window and vector rendering respectively; do not introduce another window toolkit.

#### Shutdown

Release resources in reverse creation order:

1. `nvgDeleteGLES2`
2. `SDL_GL_DeleteContext`
3. `SDL_DestroyWindow`
4. `SDL_Quit`

### 4. Update `README.md`

Document:

- That the example uses SDL2 for the window/event loop and NanoVG GLES2 for drawing.
- Native prerequisites: a C/C++ compiler, CMake, Git/network access for the first configuration, SDL2 development files, and GLES2 development files.
- Build and run commands:

```sh
cmake -S . -B build
cmake --build build
./build/car-gui
```

- That NanoVG is downloaded automatically by CMake and pinned to a commit.
- That GLES2 was deliberately selected to keep the renderer compatible with a future Emscripten/WebGL 1 build.
- That WebAssembly output is future work and is not part of this task.

## Expected files

Files to modify:

- `CMakeLists.txt`
- `lib/CMakeLists.txt`
- `src/main.cpp`
- `README.md`

Likely new file:

- `src/nanovg_gles2.cpp`

A small wrapper header may also be added if it is the cleanest way to expose `nvgCreateGLES2`/`nvgDeleteGLES2` declarations without leaking compile definitions.

## Validation

Configure and compile from a clean build directory:

```sh
rm -rf build
cmake -S . -B build
cmake --build build --parallel
```

Run the executable in a graphical session and verify:

- A window opens without SDL, GLES, or NanoVG errors.
- A filled triangle is visible.
- Resizing keeps the triangle centered and correctly rendered.
- High-DPI rendering does not appear incorrectly scaled.
- Escape and the window close button terminate cleanly.
- No duplicate NanoVG implementation symbols are produced.
- Rebuilding does not repeatedly redownload NanoVG.
- The executable does not link against GLEW or a desktop-only OpenGL backend.

If graphical execution is unavailable in CI, successful configuration and compilation are the minimum automated checks; do not expect SDL's dummy video driver to provide an OpenGL context.

## Out of scope

- Producing `.wasm`, JavaScript, or HTML artifacts now.
- Adding Emscripten as a downloaded dependency.
- Fetching SDL2 for native builds.
- Fonts, images, input widgets, or a complete GUI framework.
- Automated pixel comparison tests.
- Upgrading to GLES3/WebGL 2 without a demonstrated requirement.
