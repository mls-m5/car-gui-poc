# Agent instructions

## Project documentation

Read [`README.md`](README.md) before making changes. It describes the project,
its dependencies, graphics backend, and build commands.

## Building and validation

Use the documented out-of-source build:

```sh
cmake -S . -B build
cmake --build build --parallel
```

Run `clang-format` on all changed C/C++ source and header files when the
implementation is complete, then run `git diff --check`. For example:

```sh
clang-format -i src/*.cpp src/*.h test/*.cpp
```

When graphical behavior changes, launch `./build/car-gui` in a graphical
session and verify that it starts without errors.

## Portability

Preserve the OpenGL ES 2 rendering path. It is the compatibility baseline for
the project's planned Emscripten/WebAssembly support. Avoid desktop-only
OpenGL APIs unless the portability decision is explicitly changed and
documented.

SDL2 is a system dependency for native builds. NanoVG is fetched and pinned by
CMake; do not copy its source into the repository or modify the fetched tree.

## Tasks

Task descriptions belong in `tasks/`. Once a task is implemented and
validated, move its Markdown file to `tasks/done/` instead of deleting it.
