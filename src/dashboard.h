#pragma once
#include "nanovg_gles2.h"
#include "vehicle_data.h"
struct Rect {
    float x;
    float y;
    float width;
    float height;
};
void draw_driver_display(NVGcontext *, const Rect &, const VehicleData &);
void draw_vehicle_details(NVGcontext *, const Rect &, const VehicleData &);
