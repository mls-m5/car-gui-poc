#include "dashboard.h"
#include <algorithm>
#include <cmath>
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
NVGcolor blend_color(NVGcolor from, NVGcolor to, float amount) {
    amount = std::clamp(amount, 0.f, 1.f);
    return {from.r + (to.r - from.r) * amount,
            from.g + (to.g - from.g) * amount,
            from.b + (to.b - from.b) * amount,
            from.a + (to.a - from.a) * amount};
}
void compact_turn_signal(
    NVGcontext *v, float x, float y, bool points_left, float intensity) {
    const float direction = points_left ? -1.f : 1.f;
    const NVGcolor color = blend_color(nvgRGB(42, 50, 61), green, intensity);
    auto path = [&] {
        nvgBeginPath(v);
        nvgMoveTo(v, x + direction * 14, y - 4);
        nvgLineTo(v, x, y - 4);
        nvgLineTo(v, x, y - 11);
        nvgLineTo(v, x - direction * 15, y);
        nvgLineTo(v, x, y + 11);
        nvgLineTo(v, x, y + 4);
        nvgLineTo(v, x + direction * 14, y + 4);
        nvgClosePath(v);
    };
    path();
    nvgFillColor(v, color);
    nvgFill(v);
}
void compact_headlight(
    NVGcontext *v, float x, float y, float intensity, bool high_beam) {
    const NVGcolor active_color = high_beam ? nvgRGB(59, 130, 246) : green;
    const NVGcolor color =
        blend_color(nvgRGB(42, 50, 61), active_color, intensity);
    auto paths = [&](NVGcolor stroke_color, float stroke_width) {
        nvgStrokeColor(v, stroke_color);
        nvgStrokeWidth(v, stroke_width);
        nvgBeginPath(v);
        nvgMoveTo(v, x - 12, y - 10);
        nvgBezierTo(v, x - 2, y - 7, x - 2, y + 7, x - 12, y + 10);
        nvgClosePath(v);
        nvgStroke(v);
        for (int i = -1; i <= 1; ++i) {
            nvgBeginPath(v);
            nvgMoveTo(v, x + 1, y + i * 7);
            nvgLineTo(v, x + 14, y + i * 7 + (high_beam ? 0 : 3));
            nvgStroke(v);
        }
    };
    nvgSave(v);
    nvgLineCap(v, NVG_ROUND);
    paths(color, 2.f + .6f * intensity);
    nvgRestore(v);
}
void compact_warning(
    NVGcontext *v, float x, float y, float intensity, bool tire) {
    const NVGcolor color =
        blend_color(nvgRGB(42, 50, 61), tire ? amber : red, intensity);
    nvgSave(v);
    nvgStrokeColor(v, color);
    nvgFillColor(v, color);
    nvgStrokeWidth(v, 2.3f);
    nvgLineCap(v, NVG_ROUND);
    nvgLineJoin(v, NVG_ROUND);
    nvgBeginPath(v);
    if (tire) {
        nvgMoveTo(v, x - 12, y - 10);
        nvgBezierTo(v, x - 17, y, x - 11, y + 12, x - 5, y + 12);
        nvgLineTo(v, x + 5, y + 12);
        nvgBezierTo(v, x + 11, y + 12, x + 17, y, x + 12, y - 10);
    }
    else {
        nvgMoveTo(v, x, y - 13);
        nvgLineTo(v, x - 14, y + 12);
        nvgLineTo(v, x + 14, y + 12);
        nvgClosePath(v);
    }
    nvgStroke(v);
    txt(v, "!", x, y + 3, 15, color, NVG_ALIGN_CENTER);
    nvgRestore(v);
}
void circular_gauge(NVGcontext *v,
                    float x,
                    float y,
                    float percentage,
                    NVGcolor color,
                    const char *value,
                    const char *unit,
                    bool clockwise) {
    constexpr float pi = 3.14159265f;
    percentage = std::clamp(percentage, 0.f, 1.f);
    nvgSave(v);
    nvgLineCap(v, NVG_ROUND);
    nvgBeginPath(v);
    nvgCircle(v, x, y, 90);
    nvgStrokeWidth(v, 8);
    nvgStrokeColor(v, nvgRGBA(255, 255, 255, 20));
    nvgStroke(v);
    if (percentage > 0.001f) {
        const float arc_end =
            -pi / 2 + (clockwise ? 1.f : -1.f) * percentage * 2 * pi;
        const int direction = clockwise ? NVG_CW : NVG_CCW;
        nvgBeginPath(v);
        nvgArc(v, x, y, 90, -pi / 2, arc_end, direction);
        nvgStrokeWidth(v, 8);
        nvgStrokeColor(v, color);
        nvgStroke(v);
    }
    txt(v, value, x, y - 10, 52, white, NVG_ALIGN_CENTER);
    nvgTextLetterSpacing(v, 1.5f);
    txt(v, unit, x, y + 40, 12, muted, NVG_ALIGN_CENTER);
    nvgTextLetterSpacing(v, 0);
    nvgRestore(v);
}
void draw_modern_driver_display(NVGcontext *v,
                                const Rect &bounds,
                                const VehicleData &d,
                                DriverDashboardAnimation *animation) {
    double displayed_power_kw = d.power_kw;
    if (animation) {
        if (animation->last_power_update_seconds < 0)
            animation->last_power_update_seconds = d.simulation_time_seconds;
        const double dt = std::clamp(d.simulation_time_seconds -
                                         animation->last_power_update_seconds,
                                     0.0,
                                     0.1);
        animation->displayed_power_kw +=
            (d.power_kw - animation->displayed_power_kw) *
            std::min(1.0, dt * 4.0);
        animation->last_power_update_seconds = d.simulation_time_seconds;
        displayed_power_kw = animation->displayed_power_kw;
    }
    const std::array<float, 6> indicator_targets = {
        d.warnings.left_indicator ? 1.f : 0.f,
        d.warnings.headlights && !d.warnings.high_beam ? 1.f : 0.f,
        d.warnings.high_beam ? 1.f : 0.f,
        d.warnings.tire_pressure ? 1.f : 0.f,
        d.warnings.general_warning || d.warnings.battery_warning ? 1.f : 0.f,
        d.warnings.right_indicator ? 1.f : 0.f};
    std::array<float, 6> indicator_intensity = indicator_targets;
    if (animation) {
        if (animation->last_indicator_update_seconds < 0)
            animation->last_indicator_update_seconds =
                d.simulation_time_seconds;
        const double indicator_dt =
            std::clamp(d.simulation_time_seconds -
                           animation->last_indicator_update_seconds,
                       0.0,
                       0.1);
        for (std::size_t i = 0; i < indicator_targets.size(); ++i)
            animation->indicator_intensity[i] +=
                (indicator_targets[i] - animation->indicator_intensity[i]) *
                std::min(1.0, indicator_dt * 7.0);
        animation->last_indicator_update_seconds = d.simulation_time_seconds;
        indicator_intensity = animation->indicator_intensity;
    }
    const float sx = std::min(bounds.width / 800.f, bounds.height / 480.f);
    const float ox = bounds.x + (bounds.width - 800 * sx) / 2;
    const float oy = bounds.y + (bounds.height - 480 * sx) / 2;
    nvgSave(v);
    nvgScissor(v, bounds.x, bounds.y, bounds.width, bounds.height);
    nvgTranslate(v, ox, oy);
    nvgScale(v, sx, sx);
    nvgBeginPath(v);
    nvgRect(v, 0, 0, 800, 480);
    nvgFillColor(v, nvgRGB(10, 12, 16));
    nvgFill(v);
    nvgBeginPath(v);
    nvgRoundedRect(v, 18, 16, 764, 448, 24);
    nvgFillColor(v, nvgRGBA(255, 255, 255, 8));
    nvgFill(v);
    nvgStrokeColor(v, nvgRGBA(255, 255, 255, 14));
    nvgStrokeWidth(v, 1);
    nvgStroke(v);

    compact_turn_signal(v, 260, 58, false, indicator_intensity[0]);
    compact_headlight(v, 316, 58, indicator_intensity[1], false);
    compact_headlight(v, 372, 58, indicator_intensity[2], true);
    compact_warning(v, 428, 58, indicator_intensity[3], true);
    compact_warning(v, 484, 58, indicator_intensity[4], false);
    compact_turn_signal(v, 540, 58, true, indicator_intensity[5]);
    nvgBeginPath(v);
    nvgMoveTo(v, 55, 92);
    nvgLineTo(v, 745, 92);
    nvgStrokeColor(v, nvgRGBA(255, 255, 255, 13));
    nvgStrokeWidth(v, 1);
    nvgStroke(v);

    char speed[24], range[24], soc[24], trip[48], temperature[32];
    std::snprintf(speed, sizeof speed, "%.0f", d.speed_kph);
    std::snprintf(range, sizeof range, "%.0f", d.estimated_range_km);
    circular_gauge(v,
                   225,
                   225,
                   (float)d.speed_kph / 220.f,
                   nvgRGB(0, 242, 254),
                   speed,
                   "KM/H",
                   false);
    const bool regenerating = displayed_power_kw < -0.05;
    const float power_limit =
        (float)(regenerating ? d.available_regen_power_kw
                             : d.available_discharge_power_kw);
    circular_gauge(v,
                   575,
                   225,
                   (float)std::abs(displayed_power_kw) /
                       (std::max(1.f, power_limit) * 4.f),
                   regenerating ? nvgRGB(239, 68, 68) : nvgRGB(0, 242, 254),
                   range,
                   "KM REMAINING",
                   !regenerating);
    txt(v, gear_name(d.gear), 400, 225, 26, cyan, NVG_ALIGN_CENTER);

    nvgTextLetterSpacing(v, 1.f);
    txt(v, "BATTERY", 200, 369, 13, muted);
    nvgTextLetterSpacing(v, 0);
    std::snprintf(soc, sizeof soc, "%.0f%%", d.battery_soc_percent);
    txt(v, soc, 600, 369, 14, white, NVG_ALIGN_RIGHT);
    const NVGcolor battery_color = d.battery_soc_percent <= 15   ? red
                                   : d.battery_soc_percent <= 30 ? amber
                                                                 : green;
    box(v, 200, 394, 400, 8, nvgRGBA(255, 255, 255, 20));
    const float battery_width =
        400 * std::clamp((float)d.battery_soc_percent / 100.f, 0.f, 1.f);
    if (battery_width > 0) {
        box(v, 200, 394, battery_width, 8, battery_color);
    }
    std::snprintf(trip, sizeof trip, "TRIP  %.1f KM", d.trip_distance_km);
    std::snprintf(
        temperature, sizeof temperature, "%.0f °C", d.outside_temperature_c);
    txt(v, trip, 200, 430, 12, muted);
    txt(v, temperature, 600, 430, 12, muted, NVG_ALIGN_RIGHT);
    if (d.warnings.seat_belt)
        txt(v, "SEAT BELT", 55, 430, 11, red);
    if (d.warnings.parking_brake)
        txt(v, "PARK", 745, 430, 11, red, NVG_ALIGN_RIGHT);
    nvgRestore(v);
}
} // namespace
void draw_legacy_driver_display(NVGcontext *v,
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
void draw_driver_display(NVGcontext *v,
                         const Rect &bounds,
                         const VehicleData &d,
                         DriverDashboardStyle style,
                         DriverDashboardAnimation *animation) {
    if (style == DriverDashboardStyle::legacy)
        draw_legacy_driver_display(v, bounds, d);
    else
        draw_modern_driver_display(v, bounds, d, animation);
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
