# Create a two-window electric-car dashboard proof of concept

## Objective

Replace the current triangle demonstration with a polished proof-of-concept electric-car dashboard rendered with SDL2 and NanoVG.

One native executable, `car-gui`, should open **two independent windows**:

1. **Driver display** — large, immediately useful driving information such as speed, gear, battery state, range, current power/regen, and warning indicators.
2. **Vehicle details display** — lower-priority engineering and trip information such as battery voltage/current/temperature, cell spread, state of health, motor and inverter temperatures, tire pressures, and accumulated energy.

The values will initially come from a deterministic simulated backend. Rendering code must only consume a data snapshot and must not know how the values were produced. This boundary is important because the simulator will eventually be replaced by a backend reading real vehicle data.

This task is a visual POC, not a safety-certified instrument cluster. It must not imply that simulated values are real vehicle measurements.

## Existing project constraints

Read `AGENTS.md` and `README.md` before implementation.

Preserve the existing choices:

- C++17
- SDL2 for windows and events
- NanoVG for drawing
- OpenGL ES 2 / NanoVG GLES2 renderer
- CMake `FetchContent` integration for NanoVG
- Future Emscripten/WebAssembly compatibility

Do not introduce Qt, ImGui, GLFW, desktop-only OpenGL, or another rendering framework.

The existing triangle can be removed once the dashboard replaces it.

## Important WebAssembly design constraint

Native SDL2 supports two top-level windows, but an Emscripten application normally renders to one browser canvas and cannot directly reproduce two normal desktop windows.

Implement two reusable panel renderers that are independent of SDL windows:

```cpp
void draw_driver_display(NVGcontext* vg, const Rect& bounds, const VehicleData& data);
void draw_vehicle_details(NVGcontext* vg, const Rect& bounds, const VehicleData& data);
```

The native shell should call each renderer in a separate SDL window. Do not put SDL window operations inside the panel renderers. A future browser shell can then render the panels as tabs, side-by-side regions, or separate canvases without rewriting the dashboard UI or backend.

Keep the current callable per-frame architecture. Native code may use a normal loop; a future Emscripten entry point must be able to call the same update/render operation through `emscripten_set_main_loop_arg` without Asyncify.

A working Emscripten build is not required in this task.

## Suggested source organization

Do not leave the whole implementation in `main.cpp`. Use approximately this structure; modest naming changes are acceptable if responsibilities remain clear:

```text
src/
  main.cpp                         application startup and native loop
  application.h
  application.cpp                 SDL lifecycle, events, two displays
  graphics_display.h
  graphics_display.cpp            one SDL window + GL/NanoVG context
  vehicle_data.h                  snapshot types and backend interface
  simulated_vehicle_data.h
  simulated_vehicle_data.cpp      all dummy-data formulas
  dashboard_theme.h               colors and shared dimensions
  dashboard_draw.h
  dashboard_draw.cpp              reusable drawing helpers
  driver_display.h
  driver_display.cpp              primary panel renderer
  vehicle_details_display.h
  vehicle_details_display.cpp     secondary panel renderer
  nanovg_gles2.h/.cpp             existing GLES2 integration
assets/fonts/
  <regular font>.ttf
  <bold font>.ttf
  LICENSE or source/license note
```

Avoid unnecessary inheritance in the UI. The backend interface is the important abstraction.

Update `CMakeLists.txt` with all new source files and asset handling. Keep compiler warnings clean.

## Data/backend design

### Snapshot type

Create a plain `VehicleData` value type containing everything a frame needs. Use explicit units in member names or nearby comments. Do not pass raw arrays of unrelated numbers.

Suggested fields:

```cpp
enum class Gear { park, reverse, neutral, drive };
enum class DriveMode { eco, normal, sport };
enum class ChargeState { disconnected, charging, complete };

struct WarningStates {
    bool left_indicator;
    bool right_indicator;
    bool high_beam;
    bool headlights;
    bool seat_belt;
    bool parking_brake;
    bool tire_pressure;
    bool battery_warning;
    bool general_warning;
};

struct VehicleData {
    double simulation_time_seconds;

    // Immediate driver information
    double speed_kph;
    Gear gear;
    DriveMode drive_mode;
    double battery_soc_percent;
    double estimated_range_km;
    double power_kw;                 // Positive = traction, negative = regeneration
    double consumption_kwh_per_100km;
    double trip_distance_km;
    double odometer_km;
    double outside_temperature_c;
    WarningStates warnings;

    // Battery and electrical details
    ChargeState charge_state;
    double battery_soh_percent;
    double pack_voltage_v;
    double pack_current_a;           // Sign convention must match power_kw
    double battery_temperature_c;
    double battery_min_temperature_c;
    double battery_max_temperature_c;
    double minimum_cell_voltage_v;
    double maximum_cell_voltage_v;
    double cell_voltage_delta_mv;
    double twelve_volt_voltage_v;
    double available_discharge_power_kw;
    double available_regen_power_kw;

    // Drivetrain/chassis details
    double motor_temperature_c;
    double inverter_temperature_c;
    double tire_pressure_front_left_bar;
    double tire_pressure_front_right_bar;
    double tire_pressure_rear_left_bar;
    double tire_pressure_rear_right_bar;

    // Trip/efficiency details
    double trip_energy_used_kwh;
    double trip_energy_regenerated_kwh;
    double average_consumption_kwh_per_100km;
};
```

Fields may be grouped into nested structs if that improves readability. Keep one immutable snapshot per rendered application frame so both windows show matching data.

### Swappable backend interface

Define a narrow interface, for example:

```cpp
class VehicleDataSource {
public:
    virtual ~VehicleDataSource() = default;
    virtual VehicleData sample(double elapsed_seconds) = 0;
};
```

Implement `SimulatedVehicleDataSource` behind this interface. `Application` should own a `std::unique_ptr<VehicleDataSource>` or receive one through its constructor. `main.cpp` should explicitly construct the simulator and inject it.

A future CAN bus, serial, network, or recorded-file backend must be replaceable without editing either display renderer. Do not expose SDL or NanoVG types from the backend interface.

Only the simulator may contain sine-wave/time formulas. Do not calculate fake values in drawing functions.

### Simulation behavior

Use `SDL_GetPerformanceCounter`/`SDL_GetPerformanceFrequency` or `std::chrono::steady_clock` for monotonic elapsed time. Never use wall-clock time for animation.

Make the simulation deterministic for a given elapsed time. Avoid per-frame random values, which flicker and make testing difficult. Clamp all generated values to credible display ranges.

Use smooth combinations of sine waves with different periods. Suggested behavior:

- Speed smoothly travels through roughly `0–125 km/h` over a 25–40 second cycle.
- Gear is normally `D`; briefly show `P`, `R`, and `N` during a longer cycle so all states can be seen. Force speed near zero while showing `P` or `R` if practical.
- Traction power ranges approximately `0–110 kW`; regeneration ranges approximately `-45–0 kW`. Derive it partly from speed change so deceleration tends to display regen.
- SOC moves visibly but slowly through roughly `25–90%`. This accelerated SOC change is for demonstration only.
- Estimated range should broadly track SOC, approximately `80–420 km`.
- Instant consumption can range from `8–30 kWh/100 km`; avoid division by speed near zero.
- Pack voltage can vary around `350–410 V`.
- Pack current should have the same sign convention as power and be approximately `power * 1000 / voltage`.
- Battery temperatures can vary around `20–42 °C`, with min/max bracketing the average.
- Cell voltages should remain near `3.5–4.2 V`; calculate and display a plausible delta in millivolts.
- Battery SOH should remain near `94–99%` and vary only slightly.
- Motor and inverter temperatures should move smoothly and generally rise with power.
- Tire pressures should vary slightly around `2.4–2.7 bar`; periodically make one tire low enough to correspond with the tire-pressure warning.
- 12 V system voltage can vary around `13.5–14.5 V`.
- Trip distance and odometer must move forward based on simulated speed rather than oscillating backward. Keep integration state inside `SimulatedVehicleDataSource` if necessary.
- Trip energy used and regenerated should also accumulate monotonically in their respective totals.

Warning demonstrations should be obvious but not chaotic:

- Blink left indicator for part of a repeating cycle, then right indicator during another part.
- Toggle high beam/headlight state on a longer cycle.
- Show seat-belt and parking-brake warnings only while stopped or at the beginning of a cycle where possible.
- Activate tire-pressure, battery, and general-warning indicators individually for several seconds during a longer 45–90 second cycle.
- Never flash every warning at once except for an optional 1–2 second startup lamp test.

Put named constants and short comments around the simulation periods so another developer can adjust the demo easily.

## Multi-window graphics lifecycle

Create a small type representing one graphical display. Each native window needs its own:

- `SDL_Window*`
- `SDL_GLContext`
- `NVGcontext*`
- SDL window ID
- open/visible state

Create two windows from one process:

- Driver display: approximately `1100 x 600`, title `EV Driver Display`.
- Details display: approximately `850 x 650`, title `EV Vehicle Details`.

Position them with an offset so they do not completely overlap. Both must be resizable and high-DPI aware.

Each OpenGL context requires its own NanoVG context. Do not reuse one `NVGcontext` across unrelated GL contexts unless explicit GL resource sharing has been correctly implemented; resource sharing is unnecessary for this POC.

Before initializing, rendering, swapping, or deleting a display's NanoVG resources, call:

```cpp
SDL_GL_MakeCurrent(display.window, display.gl_context);
```

For every rendered display:

1. Make that display's GL context current.
2. Query logical window dimensions and drawable framebuffer dimensions.
3. Guard against minimized or zero-size windows.
4. Compute a high-DPI pixel ratio.
5. Set the GLES2 viewport to drawable dimensions.
6. Clear color and stencil buffers.
7. Call `nvgBeginFrame` with logical size and pixel ratio.
8. Invoke the display's panel renderer with logical bounds and the same `VehicleData` snapshot.
9. Call `nvgEndFrame`.
10. Swap that display's SDL window.

Process SDL events once per application frame. Use `event.window.windowID` to identify which window was closed. Closing one window should close only that display; keep running while the other remains open. Escape should close the whole application. Exit automatically once both displays are closed.

Destroy each NanoVG context while its corresponding GL context is current, then delete the GL context and window. Finally call `SDL_Quit`. Partial initialization failures must clean up all successfully created resources.

## Font/assets requirement

The dashboards require text. Add an openly licensed font with regular and bold faces under `assets/fonts/`, along with its license/source information. Inter, Roboto, or another clearly redistributable UI font is suitable. Do not depend on `/usr/share/fonts` or another machine-specific absolute path.

Load fonts once per NanoVG context with `nvgCreateFont`. Because each window has a separate NanoVG context, load the font into both. Fail initialization with a useful message if required fonts cannot be loaded.

Have CMake copy `assets/` beside the executable after building so running from the build directory works. Resolve assets relative to `SDL_GetBasePath()` rather than the process working directory. Keep paths compatible with forward slashes and a future Emscripten virtual filesystem. A later Emscripten build can preload the same `assets` directory.

Do not download fonts at application runtime.

## Shared visual language

Use a dark automotive theme inspired by modern EV instrument clusters:

- Near-black/navy background.
- Slightly lighter cards/panels with subtle borders.
- White primary text and muted blue-gray secondary labels.
- Cyan/blue for normal energy use and selected items.
- Green/teal for regeneration, healthy state, and turn indicators.
- Amber for cautions.
- Red only for urgent warnings or dangerously low values.

Centralize colors, corner radii, spacing, and typography sizes in a small theme definition. Do not scatter unrelated literal colors throughout rendering code.

Create reusable NanoVG helpers for common elements:

- Filled/stroked rounded card.
- Label/value/unit text row.
- Horizontal progress bar.
- Arc or ring gauge.
- Status/warning chip.
- Section heading.
- Battery outline and fill.
- Simple warning icons made from NanoVG primitives.

Keep helpers stateless and pass `NVGcontext*`, bounds, and values explicitly. Use `nvgSave`/`nvgRestore` around helpers that change transforms, scissor rectangles, alpha, or other persistent NanoVG state.

Do not rely on tiny text. The UI must remain readable at the default window sizes. Clip or simplify gracefully at smaller sizes rather than allowing labels to overlap.

## Driver display layout

The driver display should prioritize glanceable information. At the default size, implement approximately this layout:

### Top status strip

- Left/right turn indicators near the corresponding sides.
- Headlight/high-beam state.
- Drive mode (`ECO`, `NORMAL`, or `SPORT`).
- Outside temperature.
- Current time is optional; do not add a wall-clock dependency if omitted.

### Center speed area

- Very large integer speed in `km/h`, centered.
- Current gear shown prominently near speed. Display `P R N D` with the active gear highlighted, or show one large active gear.
- A subtle speed/power arc or radial accent is encouraged but must not make the numerical speed harder to read.

### Left energy/power area

- Signed power value in `kW`.
- A bar or arc with a center zero: green/teal to one side for regeneration and cyan/blue to the other for traction power.
- Label negative values clearly as regeneration, not consumption.
- Instant consumption in `kWh/100 km`.

### Right battery/range area

- Battery icon or ring filled according to SOC.
- SOC percentage.
- Estimated range in kilometers.
- Use amber below roughly 20% and red below roughly 10%; otherwise use the normal accent.

### Bottom status/warning area

- Trip distance and odometer.
- Seat belt, parking brake, tire pressure, battery, and general-warning indicators.
- Inactive warning icons should either be hidden or very dim. Active warnings must include both recognizable shape/text and color; do not rely on color alone.

The speed should remain the visually dominant item.

## Vehicle details display layout

Use a card/grid layout with a title such as `Vehicle & Battery Details`. At the default size, show these sections:

### Battery pack card

- SOC and SOH progress bars.
- Pack voltage, signed current, and signed power.
- Charge state.
- Available discharge and regeneration power.

### Temperature card

- Battery average/min/max temperature.
- Motor temperature.
- Inverter temperature.
- Values should change color when crossing clearly named demo thresholds.

### Cell balance card

- Minimum cell voltage.
- Maximum cell voltage.
- Delta in millivolts.
- A small visual balance bar or range indicator.

### Tire and low-voltage card

- Four tire pressures arranged around a simple top-view vehicle silhouette or FL/FR/RL/RR grid.
- Highlight the simulated low tire and correlate it with the warning state.
- Show 12 V bus voltage.

### Trip efficiency card

- Trip distance.
- Energy used.
- Energy regenerated.
- Average consumption.
- Optionally calculate/display recovered-energy percentage, with a zero guard.

A small footer such as `SIMULATED DATA • POC` should always be visible so the nature of the data is unambiguous.

## Responsiveness

Use logical window dimensions from SDL and calculate layout rectangles from the available bounds.

- Establish a design/reference size for each panel.
- Scale font sizes, spacing, stroke widths, and icon dimensions consistently.
- Prefer a bounded uniform scale and centered content over independent X/Y stretching.
- At small sizes, reduce secondary decoration before reducing critical text below readability.
- Test both wider and taller resize shapes.

Do not use framebuffer pixel dimensions as NanoVG layout coordinates; those are only for `glViewport`. NanoVG coordinates should use logical SDL dimensions and the calculated pixel ratio.

## Error handling

Report clear errors for:

- SDL initialization.
- Either window creation.
- Either GL context creation or `SDL_GL_MakeCurrent` call.
- Either NanoVG context creation.
- Font loading.

If the second display fails to initialize, clean up the first display before exiting. Vsync failure may be logged but should remain non-fatal.

## Tests

Add lightweight non-graphical tests for the backend where practical. The test target should not need an SDL window or OpenGL context.

At minimum verify several timestamps and/or update intervals for:

- SOC remains in `[0, 100]`.
- Speed is non-negative and within the selected maximum.
- Range is non-negative.
- Minimum cell voltage does not exceed maximum cell voltage.
- Reported cell delta matches min/max within reasonable floating-point tolerance.
- Battery minimum temperature does not exceed average or maximum temperature.
- Tire pressures remain positive.
- Trip distance and cumulative energy do not decrease as simulation time advances.
- Simulator output is deterministic for equivalent initial state and elapsed/update sequence.

If the existing `test/CMakeLists.txt` is empty, add a small test executable using standard assertions and register it with CTest; avoid adding a full test framework just for this POC.

## Documentation updates

Update `README.md` to explain:

- The two dashboard windows and their roles.
- That all displayed values are deterministic simulated data.
- How the `VehicleDataSource` abstraction allows a real backend later.
- Controls: Escape exits both; each close button closes its own display.
- Font asset and license location.
- Existing build/run instructions.
- GLES2/WebGL portability choice and the browser limitation around multiple native windows.

## Implementation sequence

A recommended order for a less capable implementation agent is:

1. Add the font assets and CMake asset copying; verify font loading in the existing window.
2. Define `VehicleData`, enums, warning states, and `VehicleDataSource` without UI dependencies.
3. Implement deterministic `SimulatedVehicleDataSource` and its tests.
4. Extract one-window GL/NanoVG lifecycle into `GraphicsDisplay` and verify the triangle still renders.
5. Make `Application` create two `GraphicsDisplay` instances and render a temporary distinct background in each.
6. Implement shared theme and basic drawing helpers.
7. Implement the driver display renderer using a fixed reference layout, then make it responsive.
8. Implement the details display renderer using the same snapshot and theme.
9. Add warning cycles and verify every warning can be observed.
10. Test independent window closing, Escape, resize, high DPI where available, and partial cleanup paths.
11. Update README and run all validation commands.
12. Once complete, move this file to `tasks/done/create-electric-car-dashboard-poc.md`.

## Acceptance criteria

The task is complete when all of the following are true:

- A native `car-gui` launch opens two SDL2/OpenGL ES windows.
- The first is a readable driver-focused EV display with speed as its dominant element.
- The second presents detailed battery, electrical, temperature, tire, and trip data.
- Values animate smoothly and warnings visibly cycle on and off.
- Both windows use the same `VehicleData` snapshot each application frame.
- Rendering code contains no simulation formulas and backend code contains no SDL/NanoVG rendering logic.
- Replacing `SimulatedVehicleDataSource` with another implementation requires no renderer changes.
- Each GL context owns and correctly destroys its own NanoVG context.
- Closing one window leaves the other running; Escape exits both.
- Resize and high-DPI rendering work without obvious distortion or clipping at normal sizes.
- Fonts load from project-owned, licensed assets rather than system-specific paths.
- The implementation uses only GLES2-compatible rendering.
- Backend tests pass.
- These commands succeed:

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
git diff --check
```

- Launching `./build/car-gui` in a graphical session produces no initialization or GL errors.
- `README.md` accurately describes the result.

## Exact visual layout blueprint

Use this section as the authoritative visual specification. The earlier layout
sections explain content; this section removes guesswork about placement,
relative importance, spacing, and drawing order. Small adjustments of a few
pixels are acceptable to correct font metrics, but do not rearrange the visual
hierarchy without a clear reason.

### Coordinate and scaling rules

Treat all measurements below as **logical NanoVG units**, not framebuffer
pixels.

Implement each panel in its own fixed reference coordinate system:

- Driver display reference rectangle: `1100 x 600`.
- Details display reference rectangle: `850 x 650`.

Given an arbitrary renderer `bounds`, calculate a uniform scale:

```cpp
scale = min(bounds.width / reference_width,
            bounds.height / reference_height);
content_width = reference_width * scale;
content_height = reference_height * scale;
origin_x = bounds.x + (bounds.width - content_width) * 0.5f;
origin_y = bounds.y + (bounds.height - content_height) * 0.5f;
```

Then use `nvgSave`, translate to `(origin_x, origin_y)`, apply
`nvgScale(scale, scale)`, draw only in reference coordinates, and finally call
`nvgRestore`. This preserves all proportions and also allows a future browser
shell to render either panel into a sub-rectangle.

Clear the entire actual window to the background color before applying this
transform. Any letterbox area therefore looks intentional. Do not stretch X
and Y independently. The outermost safe margin is `32` units on the driver
display and `24` units on the details display.

Use this shared spacing rhythm:

- `4`: tiny internal separation.
- `8`: icon/text or closely related label/value separation.
- `12`: normal control padding.
- `16`: card-to-card gap on the details screen.
- `18`: major-region gap on the driver screen.
- `24`: card internal padding and details-screen outer margin.
- `32`: driver-screen outer margin.

Use these baseline component dimensions:

- Main card corner radius: `16`.
- Small chip corner radius: `10` or half its height for a pill.
- Main card border: `1` unit at low opacity.
- Active/accent stroke: `2–3` units.
- Progress bar height: `10–12`.
- Touch-sized/status chip height: at least `32`.

### Typography hierarchy

Font sizes are reference-coordinate sizes and will be uniformly scaled with
the panel:

- Driver speed: `148`, bold, tabular-looking digits if the chosen font allows.
- Driver major side value: `50–56`, bold.
- Driver gear: `28–32`, bold.
- Driver range/SOC secondary value: `28–34`, bold.
- Details card primary value: `22–26`, bold.
- Card title: `16–18`, bold, uppercase or title case consistently.
- Normal label: `13–15`, regular.
- Unit/helper text: `12–14`, regular.
- Footer/microcopy: never smaller than `11` at reference size.

Use `NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE` for label/value rows and
`NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE` for gauges. Align numerical columns to
the right when values are compared vertically. Units should be visually
secondary: approximately 65–75% of the value's font size and the muted-text
color. Do not concatenate value and unit into one centered string if doing so
makes the numeric value shift horizontally as digits change.

### Common z-order

Draw every screen back-to-front in this order:

1. Flat background.
2. Optional very subtle radial/linear background glow; keep opacity below 8%.
3. Card fills.
4. Card borders and separators.
5. Inactive gauge tracks and dim warning placeholders.
6. Active gauge fills, status colors, and icons.
7. Labels and values.
8. Urgent warning chips or overlays.

Never place a decorative glow over text. All important text must have strong
contrast against a mostly uniform region.

### Driver display: exact regions

Use the following reference rectangles:

```text
Whole display                 x=0    y=0    w=1100 h=600
Top status strip              x=32   y=24   w=1036 h=52
Left power card               x=32   y=94   w=246  h=374
Center speed stage            x=296  y=88   w=508  h=386
Right battery/range card      x=822  y=94   w=246  h=374
Bottom information strip      x=32   y=488  w=1036 h=80
```

The top, center, and bottom regions must share the horizontal center line
`x=550`. The left and right cards are equal in size and mirror one another,
which makes the speed stage feel stable. Leave `18` units between each side
card and the center stage. The center stage may have no visible card fill, or
may use a fill that is subtler than the side cards; it must not look like a
third equally weighted card.

#### Driver top strip

The top strip is a quiet status line, not a row of large cards. Draw either a
single very subtle rounded background or no fill with only a bottom separator.
Reserve these subregions:

```text
Left turn indicator           center=(58, 50), visual box about 32x28
Headlight icon/status         x=92..190
Drive-mode pill               centered at (550, 50), about 126x32
Outside temperature           x=830..966, right aligned
Right turn indicator          center=(1042, 50), visual box about 32x28
```

Place the left arrow at the far left and the right arrow at the far right so
their spatial direction is immediately clear. Draw each arrow as a filled
chevron/triangle with a short stem. An inactive arrow may remain at 10–15%
opacity to prevent layout movement; an active blink uses the green accent at
full opacity.

Place headlight and high-beam symbols directly to the right of the left turn
arrow. They must not move when toggled; reserve the same area at all times.
Use a blue accent for active high beam and muted gray for ordinary headlights.
If drawing recognizable lamp icons is too time-consuming, use fixed-width
chips labeled `LIGHTS` and `HIGH`, but icon plus short label is preferred.

The drive-mode pill is the only centered item. Its color changes by mode:
`ECO` green/teal, `NORMAL` cyan, and `SPORT` amber. Outside temperature sits
symmetrically opposite the light group and is right-aligned before the right
arrow.

#### Driver center speed stage

The speed stage owns the visual focus. Use this internal placement:

```text
Gear row                      center=(550, 123)
Decorative speed arc box      x=332 y=112 w=436 h=292
Speed number center           (550, 262)
Speed unit center             (550, 343)
Small state/message line      center=(550, 387)
```

For the gear row, place `P R N D` horizontally with centers approximately at
`(496,123)`, `(532,123)`, `(568,123)`, and `(604,123)`. Inactive gears use
muted text at 30% opacity. Put a `28 x 34` rounded highlight behind the active
letter. Do not make all four gears bright.

Draw a broad, incomplete arc behind the speed number, centered around
`(550,270)`, with radius around `190`. It should run approximately from 205°
to 335°, leaving the lower middle open. Use a `10–12` unit dark track. An
optional speed progress segment may map `0–160 km/h` across the same arc in
cyan at `4–6` units, but keep it dim enough that the number remains dominant.
Do not add dense tick labels. At most use five tiny ticks at 0/40/80/120/160.

Center the integer speed at `(550,262)`. Because NanoVG's text middle depends
on font metrics, visually tune this by a few units if necessary. Put `km/h`
under it at `(550,343)`, not on the same baseline. The speed number should
occupy roughly 260–320 units of width at three digits.

Use the state/message line for a short contextual phrase such as
`READY`, `REGENERATING`, `PARKED`, or `CHARGING`. Keep it muted unless it is a
meaningful green/amber state. It must never compete with speed.

#### Driver left power card

Use `24` units internal horizontal padding. Content centers on `x=155`:

```text
Card title "POWER"            baseline/center y=121
Signed power value            center=(155,178)
Unit "kW"                     center=(155,219)
Bipolar power bar             x=58 y=251 w=194 h=16
Bar labels REGEN / DRIVE      y=279
Divider                       x=56..254 at y=307
Consumption label             center=(155,337)
Consumption value             center=(155,377)
Consumption unit              center=(155,410)
Optional efficiency caption  center=(155,441)
```

The bipolar bar has zero at `x=155`. Negative power grows leftward in green;
positive traction power grows rightward in cyan. Use maximum scales of about
`50 kW` regen and `120 kW` drive. Draw a visible 1-unit center marker. Never
map negative power to the drive side.

Show the signed power number with either a leading minus sign or a nearby
`REGEN` state. Avoid a leading plus sign for normal positive drive power.
Consumption is less important than power, so use a value size around `30`, not
the side-card major size.

#### Driver right battery/range card

Mirror the left card's internal rhythm around center `x=945`:

```text
Card title "BATTERY"          center y=121
Battery icon                  x=883 y=143 w=124 h=58
SOC value                     center=(945,236)
SOC unit/label                center=(945,271)
SOC progress bar              x=858 y=292 w=174 h=12
Divider                       x=846..1044 at y=325
Range label                   center=(945,352)
Range value                   center=(945,393)
Range unit "km RANGE"        center=(945,429)
Charge-state chip             center=(945,449), max size 150x28
```

Draw the battery body at `x=883..997` and a terminal at
`x=999..1007`, vertically centered. Inset the body fill by `5`. Fill from left
to right by SOC and clip it to the inner rounded rectangle. Keep the battery
icon, SOC percentage, and range vertically ordered; do not place range inside
the battery icon.

Use normal cyan/green above 20%, amber at or below 20%, and red at or below
10%. Apply the warning color consistently to icon fill and SOC value, but keep
labels white/muted. Only show the charge-state chip prominently when charging
or complete; disconnected can be a dim `NOT CHARGING` caption.

#### Driver bottom strip

Divide the bottom strip into three stable zones:

```text
Trip zone                     x=52..258
Warning zone                  x=278..822
Odometer zone                 x=842..1048
```

The trip zone is left aligned: small `TRIP` label above a `0.0 km` value. The
odometer zone mirrors it and is right aligned: small `ODOMETER` label above
the value. Suggested label center Y is `510`; value center Y is `543`.

Center five warning slots in the warning zone. Use fixed boxes about
`92 x 44`, separated by `8`, centered vertically near `528`. In left-to-right
order use:

1. Seat belt (`BELT`).
2. Parking brake (`BRAKE`).
3. Tire pressure (`TIRE`).
4. Battery warning (`BATTERY`).
5. General warning (`!`).

A slot must not change size when activated. Inactive slots should be borderless
and around 8–12% opacity; active slots get a tinted fill, colored border, icon,
and short text. Use amber for belt/tire/general caution and red for parking
brake while moving or urgent battery warning. If five full labels do not fit,
use a simple icon over a short 10–11 unit caption, but preserve the order.

### Details display: exact regions

Use a header, a three-column/two-row card grid, and footer:

```text
Whole display                 x=0   y=0   w=850 h=650
Header                        x=24  y=18  w=802 h=58
Battery pack card (2 columns) x=24  y=94  w=529 h=238
Temperature card              x=569 y=94  w=257 h=238
Cell balance card             x=24  y=348 w=257 h=242
Tire/12 V card                x=297 y=348 w=256 h=242
Trip efficiency card          x=569 y=348 w=257 h=242
Footer                        x=24  y=606 w=802 h=24
```

The gaps are always `16`. The battery card is deliberately twice as wide as a
normal card because SOC, SOH, and pack power are the most important technical
items. The lower three cards have equal visual weight. Card headings all start
at their top-left padding point and share corresponding baselines.

#### Details header

Place `VEHICLE & BATTERY` at `(24,39)` using 22–24 bold text. Place a muted
subtitle `Live overview` at `(24,65)`. Right-align a small `SIMULATION` pill at
`x=706..826`, vertically centered around `47`. A thin separator may run at
`y=82`; do not wrap the header in a large card.

#### Battery pack card

Use `20` units of internal padding and a vertical split around `x=310`:

```text
Title                         x=44 y=119
Left progress region          x=44..289
Right live-values region      x=330..533
Bottom capability strip       x=44..533 y=250..314
```

Left region:

- `STATE OF CHARGE` label/value on `y=153`, with value right-aligned.
- SOC progress bar at `x=44 y=164 w=245 h=11`.
- `STATE OF HEALTH` label/value on `y=195`.
- SOH progress bar at `x=44 y=206 w=245 h=11`.

Right region contains three aligned rows. Labels are left aligned and values
right aligned:

```text
PACK VOLTAGE                  y=151
PACK CURRENT                  y=184
PACK POWER                    y=217
```

Keep units beside or immediately after values using muted smaller text. Color
negative current/power green for regen and positive power cyan.

Draw a separator at `y=236`. Divide the bottom strip into three equal cells
with centers around `x=126`, `x=289`, and `x=451`:

- Charge state.
- Maximum available drive power.
- Maximum available regen power.

Each cell has a small label on `y=264` and a bold value on `y=292`. Do not use
three unrelated colored boxes; keep them as one calm summary strip separated
by subtle vertical rules.

#### Temperature card

Use content width `217` after `20` unit side padding:

```text
Title                         x=589 y=119
Battery average value         center=(697.5,164)
Battery min/max caption       center=(697.5,194)
Divider                       x=589..806 at y=211
Motor row                     y=239
Inverter row                  y=276
Temperature range bar         x=589 y=300 w=217 h=9
```

The battery average is the largest value in this card. Display min and max on
one line beneath it, for example `MIN 24°   MAX 31°`. Show motor and inverter
as left-label/right-value rows. The final range bar may map 0–100 °C and show
small markers for the three temperatures. If that becomes cluttered, use it
only for the hottest temperature.

Use white/normal below the selected caution threshold, amber above about
`65 °C`, and red above about `80 °C`. Put thresholds in named theme/data-display
constants, not unexplained comparisons in multiple helpers.

#### Cell balance card

Use a centered visual hierarchy:

```text
Title                         x=44 y=375
Delta value                   center=(152.5,420)
Caption "CELL DELTA"          center=(152.5,449)
Balance/range bar             x=48 y=471 w=209 h=12
Minimum row                   y=512
Maximum row                   y=548
Status caption                center=(152.5,573)
```

The delta in millivolts is the dominant item. Put minimum voltage left-aligned
and maximum voltage right-aligned where practical, or use two label/value rows
if that reads better. The balance bar should show a normal green/cyan zone and
turn amber as delta grows. A textual `BALANCED`/`CHECK CELLS` caption ensures
status is not conveyed by color alone.

#### Tire and 12 V card

Reserve the upper area for a simple top-view car silhouette:

```text
Title                         x=317 y=375
Vehicle silhouette            x=386 y=409 w=78 h=112
Front-left pressure center    (352,431)
Front-right pressure center   (498,431)
Rear-left pressure center     (352,497)
Rear-right pressure center    (498,497)
Divider                       x=317..533 at y=535
12 V label/value row          y=561
```

The car silhouette should be a subdued rounded body shape, center spine, and
small front marker; it is only an orientation aid. Tire values sit outside the
four corners, not over the vehicle. Label each with `FL`, `FR`, `RL`, or `RR`
in small muted text above or beside the numeric pressure.

Use a compact font around `18–20` for pressure values. Highlight only the tire
that is actually low with amber/red text and a subtle halo. Keep healthy tire
values white. The 12 V row spans the card below the divider, with label left
and voltage right.

#### Trip efficiency card

Use stacked summary rows and keep decimal precision modest:

```text
Title                         x=589 y=375
Trip distance major value     center=(697.5,418)
Trip-distance unit/caption    center=(697.5,444)
Divider                       x=589..806 at y=461
Energy used row               y=486
Energy regenerated row        y=520
Average consumption row       y=554
Recovered-energy mini bar     x=589 y=570 w=217 h=8
```

Trip distance is the largest item. Align the three row labels left at `x=589`
and values right at `x=806`. Use cyan for energy used, green for regenerated,
and white for average consumption. The mini bar is optional if recovered
percentage is unavailable, but leaving the space empty is preferable to
inventing another metric in rendering code.

#### Details footer

At `y=618`, left-align `SIMULATED DATA • POC` in amber or muted white. On the
right, optionally show `GLES2 / NANOVG`. Keep the footer outside all cards and
visually quiet. The simulated-data notice must remain legible at all times.

### Icon construction guidance

Use simple geometry so icons remain recognizable without an icon font:

- Turn signal: filled arrow made from a rectangle/stem plus triangular head.
- Battery: rounded rectangle body with a small terminal; use an exclamation
  mark for battery warning rather than changing battery fill alone.
- General warning: outlined triangle with centered exclamation mark.
- Tire warning: horseshoe/U outline with exclamation mark; if difficult, a
  fixed `TIRE` chip is acceptable.
- Parking brake: circle containing `P` with short outer brake arcs, or a
  clearly labeled `BRAKE` chip.
- Seat belt: simplified seated-person strokes plus diagonal belt, or `BELT`.
- High beam: lamp semicircle plus three horizontal rays.

Recognizability and stable positioning matter more than detailed illustration.
Do not use emoji or platform fonts for icons because their appearance is not
portable to WebAssembly.

### Visual review checklist

Before considering the graphical work complete, inspect both windows and make
these deliberate checks:

- Squint test: speed is the first item noticed on the driver display; SOC/range
  and power are the next two regions.
- Symmetry test: left and right driver cards have equal outer dimensions and
  align at top and bottom.
- Alignment test: card titles, row labels, right-aligned values, and dividers
  share consistent baselines/edges.
- Stability test: changing from one-, two-, and three-digit values does not
  move neighboring labels or resize cards.
- Warning test: active warnings are understandable without color, while
  inactive warnings do not distract.
- Density test: the details screen is denser than the driver screen, but every
  card still has visible internal breathing room.
- Contrast test: muted text remains readable; decoration never lowers numeric
  contrast.
- Resize test: both displays retain their proportions with intentional
  letterboxing and no overlap at wide, tall, and default window shapes.
- Snapshot test: related values, such as power/current and tire warning/low
  tire, agree visually in both windows during the same frame.

If the chosen font's actual metrics create collisions, first move a baseline
by up to `6` units or reduce only that text by up to 10%. Do not solve local
collisions by changing the overall card grid or reducing every font.
