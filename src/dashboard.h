#pragma once
#include "nanovg_gles2.h"
#include "vehicle_data.h"
void draw_driver_display(NVGcontext *, float w, float h, const VehicleData &);
void draw_vehicle_details(NVGcontext *, float w, float h, const VehicleData &);
