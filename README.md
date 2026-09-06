# car-gui

A proof-of-concept electric-car GUI rendered with SDL2 and NanoVG. It opens
an EV driver display and a second vehicle-details display. SDL2 provides the
windows, OpenGL ES contexts, and event loop; NanoVG provides vector rendering.

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
commit. Bullet is pinned as the `external/bullet3` Git submodule. Clone with
`--recurse-submodules`, or initialize an existing checkout with
`git submodule update --init`. SDL2 is not downloaded by this project.

## Build and run

```sh
cmake -S . -B build
cmake --build build
./build/car-gui
```

Press Escape to close all windows. Closing an individual window leaves the
others running. The default native mode uses the automatic dummy backend. The
interactive backend opens a third window and defaults to its procedural GLES2
3D road; the original NanoVG road remains available:

```sh
./build/car-gui --backend=simulator
./build/car-gui --backend=simulator --view=2d
```

Simulator controls:

- `W`/Up accelerates; `S`/Down brakes; Space applies emergency braking.
- `A`/Left and `D`/Right steer.
- `P`, `R`, `N`, and `G` select park, reverse, neutral, and drive.
- `L` toggles headlights and `H` toggles high beam.
- `B`, `T`, and `F` toggle battery, tire-pressure, and drivetrain faults.
- `X` toggles the seat belt.

All dashboard values arrive through the decoded `VehicleDataSource` boundary.
The automatic and Bullet-powered interactive simulators implement it without
exposing their model to the dashboard renderers. Acceleration, drag, braking,
regeneration, and battery energy use are derived from the rigid body's motion
and applied forces. A future `NetworkVehicleDataSource` can own
an Ethernet connection and unpack its telemetry protocol into `VehicleData`
without changing any GUI code; no real network connection is used today. The
`assets/fonts` directory contains project-owned DejaVu Sans font files and
their license.

Run the non-graphical checks with:

```sh
ctest --test-dir build --output-on-failure
```

## Browser build (Emscripten)

Native builds use two SDL windows. Browsers use one canvas, so the web build
provides `3D DRIVE`, `2D DRIVE`, `DRIVER`, `DETAILS`, and `SPLIT` views in an
in-canvas navigation bar. The Bullet-powered 3D road is selected by default.
Use the buttons, keys `1` through `5`, or `Tab` to switch views. The same
driving and fault controls listed above work while the canvas has focus.

Install and activate the official Emscripten SDK separately:

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/Tools/emsdk
cd ~/Tools/emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

Then configure and launch through a local HTTP server:

```sh
emcmake cmake -S . -B build-web -DBUILD_TESTING=OFF
cmake --build build-web --parallel
emrun build-web/car-gui.html
```

The custom `web/shell.html` provides the full-screen page. Do not open the
HTML with `file://`; packaged WebAssembly and assets must be served over HTTP.
Emscripten supplies SDL2 and WebGL, while the project continues using NanoVG's
GLES2 backend.

### Containerized browser build

The container build does not require a host Emscripten installation. It uses a
pinned official Emscripten SDK builder image and packages the result in a small
nginx image. Podman is preferred automatically; Docker is used as a fallback.

Build the image and copy the generated HTML, JavaScript, WebAssembly, and data
files to `build-web-container/` and a second copy under
`build-web-container/public/`:

```sh
./scripts/build-web-container.sh
```

The build directory also receives an executable local server script. It serves
`build-web-container/public/` with Python and accepts an optional port:

```sh
./build-web-container/serve.sh
./build-web-container/serve.sh 9000
```

Build and serve the nginx image at <http://localhost:8080>:

```sh
./scripts/build-web-container.sh --serve
```

The image is named `car-gui-web` by default. Use `CAR_GUI_WEB_IMAGE`,
`CAR_GUI_WEB_OUTPUT`, `CAR_GUI_WEB_PUBLIC`, `CAR_GUI_WEB_PORT`, or
`CONTAINER_ENGINE` to override the defaults. For example:

```sh
CAR_GUI_WEB_PORT=9000 CONTAINER_ENGINE=docker \
    ./scripts/build-web-container.sh --serve
```
