# car-gui

A minimal SDL2 and NanoVG example that opens a window and draws a triangle.
SDL2 provides the window, OpenGL ES context, and event loop. NanoVG provides
antialiased vector rendering.

The project uses NanoVG's OpenGL ES 2 backend. GLES2 is the portable baseline
for a future Emscripten/WebAssembly build because it maps to WebGL 1. A native
WebAssembly build is not included yet.

## Requirements

Native builds require:

- CMake 3.27 or newer
- A C++17 compiler
- Git and network access during the first CMake configure
- System-installed SDL2 development files
- System-installed OpenGL ES 2 development files

NanoVG is downloaded automatically by CMake and pinned to a specific upstream
commit. SDL2 is not downloaded by this project.

## Build and run

```sh
cmake -S . -B build
cmake --build build
./build/car-gui
```

Press Escape or close the window to exit.
