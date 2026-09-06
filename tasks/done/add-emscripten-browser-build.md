# Add an Emscripten/WebAssembly browser build

## Goal

Make `car-gui` build with Emscripten and launch as a polished browser application while preserving the existing native SDL2 build.

The browser build must:

- Render the existing NanoVG electric-car dashboard through WebGL.
- Use a custom, application-specific HTML shell instead of Emscripten's standard generated demonstration page.
- Work from a local HTTP server and open with `emrun`.
- Provide access to both the driver and vehicle-details views despite browsers normally exposing only one SDL canvas/window.
- Keep native behavior unchanged: native builds should still open two top-level SDL windows.
- Continue using NanoVG's GLES2 backend, which maps naturally to WebGL 1.

This task is complete only when both native and browser builds work.

## Current state and prerequisite answer

Emscripten is **not currently installed or active on this machine**: `emcc`, `emcmake`, and `emrun` are not present in `PATH`.

Install the official Emscripten SDK before implementing or validating the browser build. The recommended setup is:

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/Tools/emsdk
cd ~/Tools/emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

The exact installation directory may differ. `source emsdk_env.sh` must be run in each new shell unless the SDK's environment setup is added to the shell profile. Confirm the environment with:

```sh
emcc --version
emcmake cmake --version
emrun --help
```

The SDK supplies the compiler, WebAssembly linker, SDL2 port integration, Node.js tooling, and `emrun`. A modern browser is also required. Python 3 and Git are needed for SDK/project setup; they are already commonly available but should be checked.

Do not use a distro's native SDL2 library for the web build. Emscripten must supply its own SDL2 port.

## Why the two native windows cannot be copied directly

The native application creates two independent `SDL_Window` objects, two GLES2 contexts, and two NanoVG contexts. In a normal Emscripten browser application, SDL renders to one HTML `<canvas>`. Browser security and page architecture do not provide ordinary desktop top-level windows, and Emscripten's SDL port should not be expected to support the current two-window model.

Do **not** try to create two SDL windows in the browser and do not use `window.open()` popups. Popups are often blocked, are awkward to synchronize, and would create unnecessary WebGL/context complexity.

Use this platform-specific presentation instead:

- **Native:** retain the two independent windows exactly as today.
- **Browser:** create one SDL window, one WebGL context, and one NanoVG context.
- Put a small in-canvas navigation bar above the dashboard with three modes:
  - `DRIVER`
  - `DETAILS`
  - `SPLIT`
- Render both dashboards from the same `VehicleData` snapshot.
- On a wide browser viewport, default to `SPLIT` and place the driver display on the left and details display on the right.
- On a narrow viewport, default to `DRIVER`; users can switch between views with the navigation bar.

This keeps the browser experience within one reliable canvas while still exposing both views. It also preserves the existing clean separation between data and rendering.

## Required architecture refactor

The current `main.cpp` owns all lifecycle and contains a blocking native loop. Refactor it before adding browser conditionals. Avoid filling every function with `#ifdef __EMSCRIPTEN__`.

Suggested files:

```text
src/
  main.cpp                         platform-neutral startup selection
  application.h
  application.cpp                 shared timing, simulator, and one-frame update
  native_application.cpp          existing two-window desktop shell
  web_application.cpp             one-window browser shell and view navigation
  graphics_display.h/.cpp         SDL/GL/NanoVG display resource wrapper
  dashboard.h/.cpp                existing reusable panel rendering
web/
  shell.html                      custom Emscripten HTML shell
```

A smaller arrangement is acceptable, but the following concepts must remain separate:

1. Vehicle simulation/data source.
2. Dashboard panel rendering.
3. SDL/GL/NanoVG display resource lifecycle.
4. Native two-window presentation.
5. Browser one-canvas presentation and browser-only main-loop setup.

### Shared frame API

Create an application object with persistent lifetime and a callable frame method. For example:

```cpp
class Application {
public:
    bool initialize();
    bool frame();       // Returns false when the app should stop.
    void shutdown();
};
```

The application must sample `VehicleDataSource` only once per frame and pass that same snapshot to every visible panel.

Native entry point:

```cpp
while (app.frame()) {
    // Native pacing may happen here.
}
app.shutdown();
```

Browser entry point:

```cpp
emscripten_set_main_loop_arg(web_frame_callback, &app, 0, true);
```

The callback invokes exactly one frame and returns control to the browser. Do not use an unconditional `while` loop, `SDL_Delay`, Asyncify, or a sleep in the browser build.

Because `emscripten_set_main_loop_arg(..., true)` does not return normally, place the browser `Application` in persistent storage (`static`, heap-owned, or another explicit lifetime). If the callback asks to stop, call `emscripten_cancel_main_loop()` and perform cleanup once.

## Make dashboard renderers accept sub-rectangles

The current functions accept only width and height:

```cpp
void draw_driver_display(NVGcontext*, float width, float height, const VehicleData&);
void draw_vehicle_details(NVGcontext*, float width, float height, const VehicleData&);
```

Change them to accept an explicit logical rectangle:

```cpp
struct Rect {
    float x;
    float y;
    float width;
    float height;
};

void draw_driver_display(
    NVGcontext* vg, const Rect& bounds, const VehicleData& data);
void draw_vehicle_details(
    NVGcontext* vg, const Rect& bounds, const VehicleData& data);
```

Update each renderer's existing reference-size transformation so its origin includes `bounds.x` and `bounds.y`:

```cpp
scale = min(bounds.width / reference_width,
            bounds.height / reference_height);
origin_x = bounds.x + (bounds.width - reference_width * scale) * 0.5f;
origin_y = bounds.y + (bounds.height - reference_height * scale) * 0.5f;
```

Native callers pass the whole logical window rectangle. Browser split mode passes a separate rectangle for each panel. Keep all layout in NanoVG logical units, not WebGL backing pixels.

Each panel currently draws its own background. Clip each panel to its supplied bounds with `nvgScissor` so a panel cannot paint over the navigation bar or the other split panel. Surround clipping/transforms with `nvgSave`/`nvgRestore`.

## Browser view modes and exact layout

Define:

```cpp
enum class WebViewMode {
    driver,
    details,
    split,
};
```

The web application owns the selected mode. Do not put it in `VehicleData` or the backend.

### Canvas navigation bar

Reserve the top `56` logical canvas units for application navigation. Draw it with NanoVG before the dashboard content:

- Background: the same near-black/navy as the dashboards.
- Bottom separator at `y = 55`.
- Left title: `EV DASHBOARD`, starting at `x = 20`, vertically centered.
- Three fixed buttons on the right, each approximately `96 x 34`, with `8` units between them.
- Button order: `DRIVER`, `DETAILS`, `SPLIT`.
- Rightmost margin: `20`.
- Selected button uses the cyan accent and bright text.
- Unselected buttons use a subtle card fill and muted text.

For a canvas narrower than `700` logical units, abbreviate the title to `EV` or hide it, but keep all three buttons usable. If three `96`-unit buttons do not fit, reduce them to approximately `76` units and labels to `DRV`, `INFO`, and `BOTH`.

Handle input using SDL events:

- Mouse/touch click in a navigation-button rectangle changes mode.
- Key `1` selects driver.
- Key `2` selects details.
- Key `3` selects split.
- `Tab` cycles driver → details → split.
- Escape should not terminate the browser runtime. It may leave browser fullscreen or return to driver mode. Preserve Escape-to-exit for native builds.

The cursor should become a pointer over the buttons if this can be done without fragile JavaScript; this is optional.

### Content area

After the navigation bar, reserve:

```text
content.x      = 16
content.y      = 72
content.width  = canvas_logical_width - 32
content.height = canvas_logical_height - 88
```

Guard against tiny or zero-size content.

Single-view modes pass the entire content rectangle to the selected renderer.

For split mode on viewports at least `1100` logical units wide:

```text
gap = 16
usable_width = content.width - gap
driver_width = usable_width * 1100 / (1100 + 850)
details_width = usable_width - driver_width

driver_bounds = {content.x, content.y,
                 driver_width, content.height}
details_bounds = {content.x + driver_width + gap, content.y,
                  details_width, content.height}
```

This approximately respects the intended relative widths of the two original windows.

For split mode below `1100` logical units wide, stack the views vertically with a `16`-unit gap. Allocate roughly 48% of available height to driver and 52% to details based on their reference heights. It is acceptable for this mode to be dense; users can select a single view for readability. Do not silently omit one panel.

Default mode after startup:

- Canvas/CSS width `>= 1200`: split.
- Width `< 1200`: driver.

Do not automatically change an explicit user selection on every resize. The responsive default applies only at startup. If desired, remember the selected mode in browser `localStorage`, but this is optional and should not require exported runtime methods.

## SDL and NanoVG browser lifecycle

The browser path creates exactly one display:

1. Initialize SDL video.
2. Set GLES2 context attributes as in the native application.
3. Determine initial canvas dimensions.
4. Create one `SDL_Window` with `SDL_WINDOW_OPENGL`, `SDL_WINDOW_RESIZABLE`, and `SDL_WINDOW_ALLOW_HIGHDPI`.
5. Create one SDL GL/WebGL context.
6. Create one `nvgCreateGLES2` context.
7. Load the regular and/or bold fonts from the preloaded virtual filesystem.
8. Start the Emscripten main loop.

Every browser frame:

1. Poll SDL events.
2. Update mode from keyboard/mouse input.
3. Query current window/drawable dimensions.
4. Skip rendering if dimensions are zero.
5. Sample one data snapshot.
6. Set viewport and clear color/stencil.
7. Call `nvgBeginFrame` with logical dimensions and pixel ratio.
8. Draw browser navigation.
9. Draw one or both dashboard renderers.
10. Call `nvgEndFrame` and swap.
11. Return to the browser.

Do not make a second WebGL or NanoVG context for the details view. Both panel renderers run sequentially in the same NanoVG frame/context.

## Browser resizing and high DPI

The HTML canvas must fill the browser viewport and update when the viewport changes.

Use a small resize function in the custom shell or Emscripten HTML5 API that:

- Reads the canvas CSS size/window inner size.
- Uses `window.devicePixelRatio`, capped at `2` to avoid excessive GPU memory on very dense displays.
- Updates the canvas backing dimensions when CSS dimensions or DPR change.
- Avoids reallocating the canvas every animation frame when dimensions are unchanged.

The page CSS should make the canvas fill the available viewport. The C++ renderer already queries SDL logical/drawable sizes every frame; adjust the SDL/browser sizing integration until:

- The entire viewport is used.
- Rendering is sharp on DPR 1 and DPR 2.
- SDL pointer coordinates line up with navigation buttons.
- Resizing does not stretch an old frame.
- No resize feedback loop occurs.

Emscripten/SDL canvas sizing semantics can differ by SDK version, so validate behavior rather than assuming native SDL high-DPI behavior. If setting backing dimensions directly from JavaScript confuses SDL's logical size, centralize the correction in one browser resize helper rather than adding scale fixes to dashboard drawing code.

## Custom HTML shell

Add `web/shell.html` and pass it to Emscripten using `--shell-file`.

The page must contain the Emscripten insertion token:

```html
{{{ SCRIPT }}}
```

Use a minimal production-style page rather than Emscripten's standard shell. It should contain only:

- Correct HTML5 document metadata.
- Page title such as `EV Dashboard`.
- Dark full-viewport background.
- One canvas with id `canvas`.
- A small centered loading overlay reading `Loading dashboard…`.
- An error message area that is hidden unless startup aborts.
- The generated Emscripten script insertion.

Do not include:

- Emscripten logo.
- Spinner graphics from the default shell.
- Output textarea.
- Console/status panels.
- Default bordered canvas styling.
- Demo instructions unrelated to this application.

Suggested structure:

```html
<body>
  <main id="app">
    <canvas id="canvas" tabindex="0" aria-label="Electric vehicle dashboard"></canvas>
    <div id="loading">Loading dashboard…</div>
    <div id="error" hidden></div>
  </main>
  <script>
    var Module = {
      canvas: document.getElementById('canvas'),
      onRuntimeInitialized() {
        document.getElementById('loading').hidden = true;
        Module.canvas.focus();
      },
      onAbort(reason) {
        document.getElementById('loading').hidden = true;
        const error = document.getElementById('error');
        error.hidden = false;
        error.textContent = `Dashboard failed to start: ${reason}`;
      }
    };
  </script>
  {{{ SCRIPT }}}
</body>
```

Adapt syntax to the selected Emscripten version if necessary. Ensure `Module` is declared before the generated script runs.

CSS requirements:

```css
html, body, #app {
    width: 100%;
    height: 100%;
    margin: 0;
    overflow: hidden;
    background: #0a101b;
}
canvas {
    display: block;
    width: 100%;
    height: 100%;
    outline: none;
}
```

Also disable the canvas context menu so right-click cannot interrupt the POC. Keep accessibility metadata and visible startup errors.

The generated output should be directly named `car-gui.html`, accompanied by files such as `car-gui.js`, `car-gui.wasm`, and the asset data package. Opening the HTML through `file://` is not supported; WebAssembly and preloaded files must be served over HTTP.

## CMake changes

The current CMake runs `find_package(SDL2 REQUIRED CONFIG)` before checking `EMSCRIPTEN`. That will incorrectly look for the host SDL2 package during a web configure. Restructure dependency selection:

```cmake
if(EMSCRIPTEN)
    # No host find_package(SDL2) or find_package(OpenGL).
else()
    find_package(SDL2 REQUIRED CONFIG)
    find_package(OpenGL REQUIRED COMPONENTS GLES2)
endif()
```

For the native target, continue linking:

```cmake
SDL2::SDL2
OpenGL::GLES2
```

For Emscripten, do not link those host imported targets. Apply the SDL2 port option to the relevant compile and link steps. At minimum the final Emscripten link needs:

```text
-sUSE_SDL=2
-sMIN_WEBGL_VERSION=1
-sMAX_WEBGL_VERSION=1
-sALLOW_MEMORY_GROWTH=1
```

Keep WebGL 1/GLES2 as the compatibility baseline. Do not switch NanoVG to GL3/GLES3 merely for the web build.

Add the shell and asset packaging options with correctly quoted absolute paths. Conceptually:

```cmake
target_link_options(car-gui PRIVATE
    "-sUSE_SDL=2"
    "-sMIN_WEBGL_VERSION=1"
    "-sMAX_WEBGL_VERSION=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "--shell-file=${CMAKE_SOURCE_DIR}/web/shell.html"
    "--preload-file=${CMAKE_SOURCE_DIR}/assets@/assets"
)
set_target_properties(car-gui PROPERTIES SUFFIX ".html")
```

Emscripten command-line options containing separate arguments may need CMake's `SHELL:` form. Inspect the verbose link command if packaging or shell arguments are split incorrectly:

```sh
cmake --build build-web --verbose
```

The native `POST_BUILD copy_directory` command should remain native-only. For web builds, `--preload-file` packages assets into Emscripten's virtual filesystem instead.

The font path in web code should resolve to:

```text
/assets/fonts/DejaVuSans.ttf
```

Do not prepend a host filesystem path. Keep a small platform-aware asset-path helper so native still resolves relative to `SDL_GetBasePath()` and web resolves from `/assets`.

The existing backend test is a native command-line CTest executable. Either:

- Add `include(CTest)`/`BUILD_TESTING` and only add `test/` when `BUILD_TESTING AND NOT EMSCRIPTEN`, or
- Explicitly skip that test target under Emscripten.

Do not let the web application build fail because CMake also tries to build/run a browser-form test executable.

## Expected browser build and run commands

After activating the SDK environment:

```sh
source ~/Tools/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF
cmake --build build-web --parallel
emrun build-web/car-gui.html
```

`emrun` should start a local server and launch the default browser. It may also be useful to test manually:

```sh
python3 -m http.server --directory build-web 8000
```

Then open:

```text
http://localhost:8000/car-gui.html
```

Never validate by double-clicking `car-gui.html`; `file://` often prevents loading `.wasm` and packaged assets.

## Native regression requirements

The web work must not break native behavior. After the refactor, the following must still work without Emscripten active:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/car-gui
```

Native acceptance remains:

- Two windows open.
- Each owns its own GLES2 and NanoVG context.
- Closing one leaves the other running.
- Escape closes both.
- Both display the same sampled data for each frame.

## Browser validation

Test at least current Firefox and Chromium if available. In browser developer tools, verify:

- No uncaught JavaScript exceptions.
- No failed `.wasm`, `.data`, font, or SDL asset requests.
- No WebGL errors or shader compilation errors.
- No host absolute paths appear in asset requests.
- Canvas resizes to the viewport.
- Rendering is sharp at normal and high DPI.
- Mouse/touch hit testing matches all navigation buttons.
- Driver, details, and split modes all render.
- Both panels in split mode show one consistent data snapshot.
- Animation continues when changing modes and resizing.
- The page contains no Emscripten default logo, output console, or default status UI.
- Reloading the page starts cleanly.

Browsers throttle animation in background tabs; that is expected and should not be treated as a failure.

## README changes

Document:

- Emscripten SDK installation/activation.
- Native and web build commands separately.
- The need to serve output over HTTP.
- `emrun` launch command.
- Browser controls (`1`, `2`, `3`, Tab, and canvas buttons).
- Native dual-window behavior versus browser single-canvas views.
- Output files generated in `build-web`.
- The fact that Emscripten is not a normal runtime dependency for native builds.

## Implementation sequence

1. Install/activate Emscripten and record `emcc --version` used for validation.
2. Introduce `Rect` bounds and update dashboard renderers/native callers without changing native visuals.
3. Extract SDL/GL/NanoVG display ownership from `main.cpp`.
4. Extract one shared frame/update operation that samples data once.
5. Confirm native two-window behavior still works before adding web code.
6. Add the Emscripten CMake dependency branch; stop finding host SDL/OpenGL for web.
7. Add one-window `WebApplication` and Emscripten main-loop callback.
8. Add in-canvas navigation and the driver/details/split layouts.
9. Add custom `web/shell.html` and responsive canvas sizing.
10. Package `/assets` with `--preload-file` and fix font resolution.
11. Build with `emcmake`, serve with `emrun`, and inspect browser developer tools.
12. Run native build/tests again.
13. Run `clang-format` on all changed C/C++ files and `git diff --check`.
14. Update README.
15. Once all acceptance criteria pass, move this task to `tasks/done/add-emscripten-browser-build.md`.

## Acceptance criteria

- `emcmake cmake` configures without looking for system SDL2 or system OpenGL.
- The Emscripten build produces `build-web/car-gui.html`, JavaScript, WebAssembly, and packaged assets.
- `emrun build-web/car-gui.html` launches the dashboard in a browser.
- The page uses the custom full-screen dark shell, not Emscripten's standard page.
- Fonts load from Emscripten's `/assets` virtual filesystem.
- Browser rendering uses one SDL window, one WebGL context, and one NanoVG context.
- Browser users can select driver, details, and split views via visible canvas controls and keyboard shortcuts.
- Wide screens default to side-by-side split; narrow screens default to the driver view.
- Browser animation uses `emscripten_set_main_loop_arg` and does not block/sleep.
- Canvas resize, DPR rendering, and input hit testing work.
- Browser console/network panels show no relevant errors.
- Native builds still open and manage two windows exactly as before.
- Native CTest passes.
- All changed C/C++ files have been run through `clang-format`.
- `git diff --check` passes.
- README contains complete setup, build, serving, and control instructions.
