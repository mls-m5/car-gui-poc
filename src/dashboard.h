#pragma once
#include "nanovg_gles2.h"
#include "vehicle_data.h"
#include <array>
struct Rect {
    float x;
    float y;
    float width;
    float height;
};
enum class DriverDashboardStyle { modern_rings, legacy };
struct DriverDashboardAnimation {
    double displayed_power_kw = 0;
    double last_power_update_seconds = -1;
    std::array<float, 8> indicator_intensity{};
    double last_indicator_update_seconds = -1;
};
void draw_driver_display(
    NVGcontext *,
    const Rect &,
    const VehicleData &,
    DriverDashboardStyle = DriverDashboardStyle::modern_rings,
    DriverDashboardAnimation * = nullptr);
void draw_vehicle_details(NVGcontext *, const Rect &, const VehicleData &);
