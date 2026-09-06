#include "dashboard.h"
#include <algorithm>
#include <cstdio>
namespace {
NVGcolor bg = nvgRGB(10, 16, 27), card = nvgRGB(20, 30, 46),
         white = nvgRGB(235, 242, 250), muted = nvgRGB(135, 157, 180),
         cyan = nvgRGB(52, 190, 245), green = nvgRGB(49, 220, 155),
         amber = nvgRGB(245, 180, 55), red = nvgRGB(245, 75, 80);
void box(NVGcontext *v, float x, float y, float w, float h, NVGcolor c) {
    nvgBeginPath(v);
    nvgRoundedRect(v, x, y, w, h, 16);
    nvgFillColor(v, c);
    nvgFill(v);
}
void txt(NVGcontext *v,
         const char *s,
         float x,
         float y,
         float size,
         NVGcolor c,
         int align = NVG_ALIGN_LEFT) {
    nvgFontSize(v, size);
    nvgFontFace(v, "regular");
    nvgFillColor(v, c);
    nvgTextAlign(v, align | NVG_ALIGN_MIDDLE);
    nvgText(v, x, y, s, nullptr);
}
void val(NVGcontext *v,
         float x,
         float y,
         double n,
         const char *unit,
         NVGcolor c = white) {
    char b[48];
    std::snprintf(b, sizeof b, "%.1f", n);
    txt(v, b, x, y, 25, c, NVG_ALIGN_RIGHT);
    txt(v, unit, x + 7, y, 12, muted);
}
void title(NVGcontext *v, const char *s, float x, float y) {
    txt(v, s, x, y, 16, muted);
}
void trip_metric(NVGcontext *v,
                 const char *label,
                 float y,
                 double number,
                 const char *unit,
                 NVGcolor color = white) {
    constexpr float right_edge = 806;
    txt(v, label, 589, y, 11, muted);
    nvgFontSize(v, 12);
    nvgFontFace(v, "regular");
    nvgTextAlign(v, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    const float unit_width = nvgTextBounds(v, 0, y, unit, nullptr, nullptr);
    val(v, right_edge - unit_width - 7, y, number, unit, color);
}
void bar(NVGcontext *v, float x, float y, float w, float p, NVGcolor c) {
    box(v, x, y, w, 10, nvgRGB(42, 55, 72));
    nvgBeginPath(v);
    nvgRoundedRect(v, x, y, w * std::clamp(p, 0.f, 1.f), 10, 5);
    nvgFillColor(v, c);
    nvgFill(v);
}
void chip(NVGcontext *v,
          const char *s,
          float x,
          float y,
          bool on,
          NVGcolor c = amber) {
    box(v,
        x,
        y,
        88,
        34,
        on ? nvgRGBA(c.r * 255, c.g * 255, c.b * 255, 45)
           : nvgRGBA(100, 120, 145, 20));
    txt(v, s, x + 44, y + 17, 11, on ? c : muted, NVG_ALIGN_CENTER);
}
void turn_signal(NVGcontext *v,
                 float center_x,
                 float center_y,
                 bool points_left,
                 bool active) {
    const float direction = points_left ? -1.f : 1.f;
    nvgBeginPath(v);
    nvgMoveTo(v, center_x + direction * 14, center_y - 4);
    nvgLineTo(v, center_x, center_y - 4);
    nvgLineTo(v, center_x, center_y - 11);
    nvgLineTo(v, center_x - direction * 15, center_y);
    nvgLineTo(v, center_x, center_y + 11);
    nvgLineTo(v, center_x, center_y + 4);
    nvgLineTo(v, center_x + direction * 14, center_y + 4);
    nvgClosePath(v);
    nvgFillColor(v, active ? green : nvgRGBA(135, 157, 180, 35));
    nvgFill(v);
}
void headlight_symbol(NVGcontext *v, bool headlights, bool high_beam) {
    const NVGcolor color = high_beam    ? cyan
                           : headlights ? white
                                        : nvgRGBA(135, 157, 180, 35);
    nvgSave(v);
    nvgStrokeColor(v, color);
    nvgStrokeWidth(v, 2.2f);
    nvgLineCap(v, NVG_ROUND);
    nvgBeginPath(v);
    nvgMoveTo(v, 109, 39);
    nvgBezierTo(v, 119, 41, 124, 45, 124, 50);
    nvgBezierTo(v, 124, 55, 119, 59, 109, 61);
    nvgClosePath(v);
    nvgStroke(v);
    for (int i = -1; i <= 1; ++i) {
        const float y = 50 + i * 7;
        nvgBeginPath(v);
        nvgMoveTo(v, 130, y);
        nvgLineTo(v, 151, high_beam ? y : y + 3);
        nvgStroke(v);
    }
    nvgRestore(v);
}
enum class WarningIcon { seat_belt, parking_brake, tire, battery, general };
void warning_symbol(NVGcontext *v,
                    WarningIcon icon,
                    float center_x,
                    float center_y,
                    bool active,
                    NVGcolor active_color = amber) {
    const NVGcolor color = active ? active_color : nvgRGBA(135, 157, 180, 40);
    box(v,
        center_x - 44,
        center_y - 17,
        88,
        34,
        active ? nvgRGBA(active_color.r * 255,
                         active_color.g * 255,
                         active_color.b * 255,
                         45)
               : nvgRGBA(100, 120, 145, 14));
    nvgSave(v);
    nvgStrokeColor(v, color);
    nvgFillColor(v, color);
    nvgStrokeWidth(v, 2.2f);
    nvgLineCap(v, NVG_ROUND);
    nvgLineJoin(v, NVG_ROUND);
    if (icon == WarningIcon::seat_belt) {
        nvgBeginPath(v);
        nvgCircle(v, center_x - 6, center_y - 8, 3);
        nvgFill(v);
        nvgBeginPath(v);
        nvgMoveTo(v, center_x - 7, center_y - 3);
        nvgLineTo(v, center_x - 9, center_y + 9);
        nvgLineTo(v, center_x + 5, center_y + 9);
        nvgMoveTo(v, center_x - 5, center_y - 1);
        nvgLineTo(v, center_x + 9, center_y + 10);
        nvgMoveTo(v, center_x + 7, center_y - 8);
        nvgLineTo(v, center_x - 2, center_y + 10);
        nvgStroke(v);
    }
    else if (icon == WarningIcon::parking_brake) {
        nvgBeginPath(v);
        nvgCircle(v, center_x, center_y, 10);
        nvgStroke(v);
        txt(v, "P", center_x, center_y, 13, color, NVG_ALIGN_CENTER);
        nvgBeginPath(v);
        nvgMoveTo(v, center_x - 14, center_y - 8);
        nvgBezierTo(v,
                    center_x - 19,
                    center_y - 4,
                    center_x - 19,
                    center_y + 4,
                    center_x - 14,
                    center_y + 8);
        nvgMoveTo(v, center_x + 14, center_y - 8);
        nvgBezierTo(v,
                    center_x + 19,
                    center_y - 4,
                    center_x + 19,
                    center_y + 4,
                    center_x + 14,
                    center_y + 8);
        nvgStroke(v);
    }
    else if (icon == WarningIcon::tire) {
        nvgBeginPath(v);
        nvgMoveTo(v, center_x - 11, center_y - 9);
        nvgBezierTo(v,
                    center_x - 15,
                    center_y,
                    center_x - 11,
                    center_y + 10,
                    center_x - 5,
                    center_y + 11);
        nvgLineTo(v, center_x + 5, center_y + 11);
        nvgBezierTo(v,
                    center_x + 11,
                    center_y + 10,
                    center_x + 15,
                    center_y,
                    center_x + 11,
                    center_y - 9);
        nvgStroke(v);
        txt(v, "!", center_x, center_y + 2, 14, color, NVG_ALIGN_CENTER);
    }
    else if (icon == WarningIcon::battery) {
        nvgBeginPath(v);
        nvgRoundedRect(v, center_x - 13, center_y - 8, 25, 16, 2);
        nvgStroke(v);
        nvgBeginPath(v);
        nvgRect(v, center_x + 12, center_y - 4, 4, 8);
        nvgFill(v);
        txt(v, "!", center_x, center_y + 1, 13, color, NVG_ALIGN_CENTER);
    }
    else {
        nvgBeginPath(v);
        nvgMoveTo(v, center_x, center_y - 12);
        nvgLineTo(v, center_x - 13, center_y + 11);
        nvgLineTo(v, center_x + 13, center_y + 11);
        nvgClosePath(v);
        nvgStroke(v);
        txt(v, "!", center_x, center_y + 3, 13, color, NVG_ALIGN_CENTER);
    }
    nvgRestore(v);
}
} // namespace
void draw_driver_display(NVGcontext *v,
                         const Rect &bounds,
                         const VehicleData &d) {
    float sx = std::min(bounds.width / 1100.f, bounds.height / 600.f),
          ox = bounds.x + (bounds.width - 1100 * sx) / 2,
          oy = bounds.y + (bounds.height - 600 * sx) / 2;
    nvgSave(v);
    nvgScissor(v, bounds.x, bounds.y, bounds.width, bounds.height);
    nvgTranslate(v, ox, oy);
    nvgScale(v, sx, sx);
    nvgBeginPath(v);
    nvgRect(v, 0, 0, 1100, 600);
    nvgFillColor(v, bg);
    nvgFill(v);
    turn_signal(v, 58, 50, true, d.warnings.left_indicator);
    turn_signal(v, 1042, 50, false, d.warnings.right_indicator);
    headlight_symbol(v, d.warnings.headlights, d.warnings.high_beam);
    box(v,
        487,
        34,
        126,
        32,
        d.drive_mode == DriveMode::eco ? nvgRGBA(49, 220, 155, 45) : cyan);
    txt(v, mode_name(d.drive_mode), 550, 50, 13, white, NVG_ALIGN_CENTER);
    char temp[32];
    std::snprintf(temp, sizeof temp, "%.0f °C", d.outside_temperature_c);
    txt(v, temp, 965, 50, 14, white, NVG_ALIGN_RIGHT);
    box(v, 32, 94, 246, 374, card);
    box(v, 822, 94, 246, 374, card);
    title(v, "POWER", 56, 121);
    title(v, "BATTERY", 846, 121);
    char speed[16];
    std::snprintf(speed, sizeof speed, "%.0f", d.speed_kph);
    txt(v, speed, 550, 258, 110, white, NVG_ALIGN_CENTER);
    txt(v, "km/h", 550, 335, 18, muted, NVG_ALIGN_CENTER);
    txt(v, gear_name(d.gear), 550, 120, 30, cyan, NVG_ALIGN_CENTER);
    txt(v,
        d.power_kw < 0 ? "REGENERATING" : "READY",
        550,
        390,
        14,
        d.power_kw < 0 ? green : muted,
        NVG_ALIGN_CENTER);
    val(v, 155, 178, d.power_kw, "kW", d.power_kw < 0 ? green : cyan);
    bar(v,
        58,
        250,
        194,
        (d.power_kw + 45) / 165,
        d.power_kw < 0 ? green : cyan);
    txt(v, "REGEN                 DRIVE", 58, 278, 10, muted);
    title(v, "CONSUMPTION", 56, 337);
    val(v, 155, 378, d.consumption_kwh_per_100km, "kWh/100", white);
    nvgBeginPath(v);
    nvgRoundedRect(v, 883, 145, 114, 58, 8);
    nvgStrokeColor(v, cyan);
    nvgStrokeWidth(v, 3);
    nvgStroke(v);
    nvgBeginPath(v);
    nvgRect(v, 997, 163, 8, 22);
    nvgFillColor(v, cyan);
    nvgFill(v);
    nvgBeginPath(v);
    nvgRect(v, 888, 150, 104 * d.battery_soc_percent / 100, 48);
    nvgFillColor(v,
                 d.battery_soc_percent < 10   ? red
                 : d.battery_soc_percent < 20 ? amber
                                              : cyan);
    nvgFill(v);
    char soc[32];
    std::snprintf(soc, sizeof soc, "%.0f%%", d.battery_soc_percent);
    txt(v, soc, 945, 238, 32, white, NVG_ALIGN_CENTER);
    bar(v,
        858,
        292,
        174,
        d.battery_soc_percent / 100,
        d.battery_soc_percent < 20 ? amber : cyan);
    title(v, "RANGE", 846, 352);
    char range[32];
    std::snprintf(range, sizeof range, "%.0f", d.estimated_range_km);
    txt(v, range, 945, 393, 34, white, NVG_ALIGN_CENTER);
    txt(v, "km estimated", 945, 427, 13, muted, NVG_ALIGN_CENTER);
    title(v, "TRIP", 52, 510);
    val(v, 258, 510, d.trip_distance_km, "km", white);
    title(v, "ODOMETER", 842, 510);
    val(v, 1048, 510, d.odometer_km, "km", white);
    const WarningIcon icons[] = {WarningIcon::seat_belt,
                                 WarningIcon::parking_brake,
                                 WarningIcon::tire,
                                 WarningIcon::battery,
                                 WarningIcon::general};
    const bool active[] = {d.warnings.seat_belt,
                           d.warnings.parking_brake,
                           d.warnings.tire_pressure,
                           d.warnings.battery_warning,
                           d.warnings.general_warning};
    for (int i = 0; i < 5; ++i)
        warning_symbol(
            v, icons[i], 330 + i * 102, 545, active[i], i == 3 ? red : amber);
    nvgRestore(v);
}
void draw_vehicle_details(NVGcontext *v,
                          const Rect &bounds,
                          const VehicleData &d) {
    float sx = std::min(bounds.width / 850.f, bounds.height / 650.f),
          ox = bounds.x + (bounds.width - 850 * sx) / 2,
          oy = bounds.y + (bounds.height - 650 * sx) / 2;
    nvgSave(v);
    nvgScissor(v, bounds.x, bounds.y, bounds.width, bounds.height);
    nvgTranslate(v, ox, oy);
    nvgScale(v, sx, sx);
    nvgBeginPath(v);
    nvgRect(v, 0, 0, 850, 650);
    nvgFillColor(v, bg);
    nvgFill(v);
    txt(v, "VEHICLE & BATTERY", 24, 39, 23, white);
    txt(v, "Live simulated overview", 24, 66, 13, muted);
    chip(v, "SIMULATION", 706, 30, true, amber);
    box(v, 24, 94, 529, 238, card);
    box(v, 569, 94, 257, 238, card);
    box(v, 24, 348, 257, 242, card);
    box(v, 297, 348, 256, 242, card);
    box(v, 569, 348, 257, 242, card);
    title(v, "BATTERY PACK", 44, 119);
    title(v, "TEMPERATURES", 589, 119);
    title(v, "CELL BALANCE", 44, 375);
    title(v, "TIRES & 12V", 317, 375);
    title(v, "TRIP EFFICIENCY", 589, 375);
    title(v, "STATE OF CHARGE", 44, 153);
    val(v, 289, 153, d.battery_soc_percent, "%", white);
    bar(v, 44, 164, 245, d.battery_soc_percent / 100, cyan);
    title(v, "STATE OF HEALTH", 44, 195);
    val(v, 289, 195, d.battery_soh_percent, "%", white);
    bar(v, 44, 206, 245, d.battery_soh_percent / 100, green);
    val(v, 530, 151, d.pack_voltage_v, "V");
    val(v,
        530,
        184,
        d.pack_current_a,
        "A",
        d.pack_current_a < 0 ? green : cyan);
    val(v, 530, 217, d.power_kw, "kW", d.power_kw < 0 ? green : cyan);
    val(v, 530, 270, d.available_discharge_power_kw, "kW");
    val(v, 530, 302, d.available_regen_power_kw, "kW", green);
    val(v, 806, 164, d.battery_temperature_c, "°C");
    title(v, "BATTERY MIN/MAX", 589, 194);
    char tm[40];
    std::snprintf(tm,
                  sizeof tm,
                  "%.0f / %.0f °C",
                  d.battery_min_temperature_c,
                  d.battery_max_temperature_c);
    txt(v, tm, 697, 194, 13, muted, NVG_ALIGN_CENTER);
    val(v, 806, 239, d.motor_temperature_c, "°C");
    val(v, 806, 276, d.inverter_temperature_c, "°C");
    txt(v,
        "DELTA",
        152,
        420,
        30,
        d.cell_voltage_delta_mv > 30 ? amber : green,
        NVG_ALIGN_CENTER);
    txt(v, "CELL BALANCE (mV)", 152, 449, 12, muted, NVG_ALIGN_CENTER);
    bar(v,
        48,
        471,
        209,
        d.cell_voltage_delta_mv / 60,
        d.cell_voltage_delta_mv > 30 ? amber : green);
    val(v, 257, 512, d.minimum_cell_voltage_v, "V");
    val(v, 257, 548, d.maximum_cell_voltage_v, "V");
    txt(v,
        d.cell_voltage_delta_mv < 30 ? "BALANCED" : "CHECK CELLS",
        152,
        574,
        12,
        d.cell_voltage_delta_mv < 30 ? green : amber,
        NVG_ALIGN_CENTER);
    const char *labels[] = {"FL", "FR", "RL", "RR"};
    double tires[] = {d.tire_pressure_front_left_bar,
                      d.tire_pressure_front_right_bar,
                      d.tire_pressure_rear_left_bar,
                      d.tire_pressure_rear_right_bar};
    float xs[] = {352, 498, 352, 498}, ys[] = {431, 431, 497, 497};
    for (int i = 0; i < 4; i++) {
        txt(v, labels[i], xs[i], ys[i] - 13, 11, muted, NVG_ALIGN_CENTER);
        char b[20];
        std::snprintf(b, sizeof b, "%.2f", tires[i]);
        txt(v,
            b,
            xs[i],
            ys[i] + 8,
            18,
            tires[i] < 2.2 ? amber : white,
            NVG_ALIGN_CENTER);
    }
    val(v, 533, 561, d.twelve_volt_voltage_v, "V");
    txt(v, "TRIP DISTANCE", 589, 418, 12, muted);
    char b[32];
    std::snprintf(b, sizeof b, "%.1f km", d.trip_distance_km);
    txt(v, b, 697, 438, 27, white, NVG_ALIGN_CENTER);
    trip_metric(v, "ENERGY USED", 486, d.trip_energy_used_kwh, "kWh");
    trip_metric(
        v, "REGENERATED", 520, d.trip_energy_regenerated_kwh, "kWh", green);
    trip_metric(
        v, "AVERAGE", 554, d.average_consumption_kwh_per_100km, "kWh/100");
    txt(v, "SIMULATED DATA • POC", 24, 618, 12, amber);
    txt(v, "GLES2 / NANOVG", 826, 618, 11, muted, NVG_ALIGN_RIGHT);
    nvgRestore(v);
}
