# Add an interactive driving simulator backend and view

## Goal

Add an isolated interactive vehicle simulation as an alternative to the existing sine-wave backend. Native simulator mode opens a third road-view window. The Emscripten build uses the interactive simulator by default and exposes the road view in its single-canvas navigation.

## Design

- Keep `VehicleDataSource` as the dashboard-facing telemetry boundary.
- Implement vehicle physics and controls in `InteractiveVehicleSimulator`; it must not depend on SDL or NanoVG.
- Expose a separate plain `SimulatorVisualState` to a NanoVG road renderer.
- A future Ethernet backend can implement `VehicleDataSource` without changing dashboard rendering. No browser networking is required now.
- Keep `SimulatedVehicleDataSource` selectable for the original automatic demo.

## Behavior

- Model acceleration, motor power limits, aerodynamic drag, rolling resistance, braking, regenerative braking, battery energy, range, and temperatures.
- Controls: W/Up throttle, S/Down brake, A/Left and D/Right steering, Space emergency brake, P park, R reverse, N neutral, G drive, L headlights, H high beam, B battery fault, T tire fault, F general drivetrain fault, X seat belt.
- Faults and controls must update the existing dashboard telemetry and warnings.
- Render a pseudo-3D road, moving lane markings, pine trees, mountains, roadside signs, vehicle, steering movement, and headlight beams.
- Native defaults to the dummy backend; `--backend=simulator` enables the third window.
- Emscripten defaults to the interactive simulator and a `SIMULATOR` view. Keep driver/details/split modes available.

## Validation

Build/test native mode, launch both native backend modes, build the containerized Emscripten target, run clang-format, and run `git diff --check`. Move this task to `tasks/done/` afterward.
