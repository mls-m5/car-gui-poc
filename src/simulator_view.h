#pragma once

#include "dashboard.h"
#include "interactive_vehicle_simulator.h"

void draw_simulator_hud(NVGcontext *vg,
                        const Rect &bounds,
                        const SimulatorVisualState &state);
