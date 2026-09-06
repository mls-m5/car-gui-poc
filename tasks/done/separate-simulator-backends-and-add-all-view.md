# Separate simulator backends and add a combined browser view

## Goal

Correct the simulator architecture so the retained 2D simulator and new 3D simulator are independent backends rather than two renderers over one model.

- Keep `TwoDVehicleSimulator` as the lightweight non-Bullet model.
- Keep `BulletVehicleSimulator` as the 3D physics and energy model.
- Have both implement the common control and `VehicleDataSource` boundaries.
- Make steering update Bullet heading and motion, and render the car body plus steerable front wheels with that orientation.
- Replace browser `SPLIT` with `ALL`, showing the 3D scene above both dashboard panels using Bullet telemetry.
- Preserve 3D as the browser and native interactive default.

Validate native dummy, 2D, and 3D modes, CTest, containerized Emscripten compilation, clang-format, and `git diff --check`, then move this file to `tasks/done/`.
