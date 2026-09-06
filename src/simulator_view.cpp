#include "simulator_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
void fill_path(NVGcontext *vg, NVGcolor color) {
    nvgFillColor(vg, color);
    nvgFill(vg);
}
void text(NVGcontext *vg,
          const char *value,
          float x,
          float y,
          float size,
          NVGcolor color,
          int alignment = NVG_ALIGN_LEFT) {
    nvgFontFace(vg, "regular");
    nvgFontSize(vg, size);
    nvgTextAlign(vg, alignment | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, color);
    nvgText(vg, x, y, value, nullptr);
}
void pine_tree(NVGcontext *vg, float x, float y, float size, bool night) {
    nvgBeginPath(vg);
    nvgRect(vg, x - size * .08f, y - size * .1f, size * .16f, size * .48f);
    fill_path(vg, nvgRGB(76, 54, 37));
    for (int layer = 0; layer < 3; ++layer) {
        const float top = y - size * (.95f - layer * .25f);
        const float half = size * (.25f + layer * .1f);
        nvgBeginPath(vg);
        nvgMoveTo(vg, x, top);
        nvgLineTo(vg, x - half, top + size * .48f);
        nvgLineTo(vg, x + half, top + size * .48f);
        nvgClosePath(vg);
        fill_path(vg, night ? nvgRGB(20, 66, 55) : nvgRGB(27, 112, 72));
    }
}
void draw_car(NVGcontext *vg, float x, float y, float steering) {
    nvgSave(vg);
    nvgTranslate(vg, x, y);
    nvgRotate(vg, steering * .08f);
    nvgBeginPath(vg);
    nvgMoveTo(vg, -43, 25);
    nvgLineTo(vg, -35, -24);
    nvgBezierTo(vg, -24, -42, 24, -42, 35, -24);
    nvgLineTo(vg, 43, 25);
    nvgBezierTo(vg, 34, 37, -34, 37, -43, 25);
    nvgClosePath(vg);
    fill_path(vg, nvgRGB(39, 151, 210));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, -26, -28, 52, 25, 8);
    fill_path(vg, nvgRGB(18, 32, 48));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, -35, 17, 18, 7, 3);
    nvgRoundedRect(vg, 17, 17, 18, 7, 3);
    fill_path(vg, nvgRGB(245, 62, 70));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, -17, 25, 34, 4, 2);
    fill_path(vg, nvgRGB(205, 224, 236));
    nvgRestore(vg);
}
} // namespace

void draw_simulator_view(NVGcontext *vg,
                         const Rect &bounds,
                         const SimulatorVisualState &state) {
    constexpr float reference_width = 1000;
    constexpr float reference_height = 650;
    const float scale = std::min(bounds.width / reference_width,
                                 bounds.height / reference_height);
    const float origin_x =
        bounds.x + (bounds.width - reference_width * scale) / 2;
    const float origin_y =
        bounds.y + (bounds.height - reference_height * scale) / 2;
    const bool night = std::fmod(state.distance_m, 1400.0) > 950;

    nvgSave(vg);
    nvgScissor(vg, bounds.x, bounds.y, bounds.width, bounds.height);
    nvgTranslate(vg, origin_x, origin_y);
    nvgScale(vg, scale, scale);

    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, reference_width, reference_height);
    fill_path(vg, night ? nvgRGB(8, 18, 38) : nvgRGB(88, 174, 221));

    const float horizon = 178;
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, horizon + 15);
    nvgLineTo(vg, 145, 100);
    nvgLineTo(vg, 260, horizon + 4);
    nvgLineTo(vg, 405, 75);
    nvgLineTo(vg, 570, horizon + 5);
    nvgLineTo(vg, 735, 105);
    nvgLineTo(vg, 1000, horizon + 18);
    nvgLineTo(vg, 1000, 250);
    nvgLineTo(vg, 0, 250);
    nvgClosePath(vg);
    fill_path(vg, night ? nvgRGB(25, 43, 61) : nvgRGB(80, 112, 126));

    nvgBeginPath(vg);
    nvgRect(vg, 0, horizon, reference_width, reference_height - horizon);
    fill_path(vg, night ? nvgRGB(15, 47, 40) : nvgRGB(49, 126, 70));

    const float bend = std::sin(state.distance_m / 240.0) * 60;
    const float horizon_center = 500 + bend;
    nvgBeginPath(vg);
    nvgMoveTo(vg, horizon_center - 55, horizon);
    nvgLineTo(vg, 60, reference_height);
    nvgLineTo(vg, 940, reference_height);
    nvgLineTo(vg, horizon_center + 55, horizon);
    nvgClosePath(vg);
    fill_path(vg, nvgRGB(46, 50, 57));

    nvgStrokeWidth(vg, 5);
    nvgStrokeColor(vg, nvgRGB(220, 222, 210));
    nvgBeginPath(vg);
    nvgMoveTo(vg, horizon_center - 55, horizon);
    nvgLineTo(vg, 60, reference_height);
    nvgMoveTo(vg, horizon_center + 55, horizon);
    nvgLineTo(vg, 940, reference_height);
    nvgStroke(vg);

    for (int i = 0; i < 13; ++i) {
        double z = std::fmod(i * 31.0 - state.distance_m, 390.0);
        if (z < 0)
            z += 390;
        const float proximity = 1.f - static_cast<float>(z / 390.0);
        const float y = horizon + proximity * proximity * 470;
        const float road_half = 55 + proximity * 390;
        const float center = 500 + bend * (1 - proximity);
        const float tree_size = 18 + proximity * 105;
        const float variation = (i % 3) * 18;
        pine_tree(vg, center - road_half - 38 - variation, y, tree_size, night);
        pine_tree(vg, center + road_half + 38 + variation, y, tree_size, night);
    }

    const double stripe_offset = std::fmod(state.distance_m, 32.0);
    for (int i = 0; i < 10; ++i) {
        double z = std::fmod(i * 42.0 - stripe_offset, 420.0);
        if (z < 5)
            z += 420;
        const float proximity = 1.f - static_cast<float>(z / 420.0);
        const float y = horizon + proximity * proximity * 480;
        const float next = std::min(1.f, proximity + .055f);
        const float y2 = horizon + next * next * 480;
        const float center = 500 + bend * (1 - proximity);
        nvgBeginPath(vg);
        nvgMoveTo(vg, center - 2 - proximity * 5, y);
        nvgLineTo(vg, center + 2 + proximity * 5, y);
        nvgLineTo(vg, center + 3 + next * 5, y2);
        nvgLineTo(vg, center - 3 - next * 5, y2);
        nvgClosePath(vg);
        fill_path(vg, nvgRGB(230, 224, 170));
    }

    if (state.headlights && night) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 455, 570);
        nvgLineTo(vg, state.high_beam ? 390 : 430, 285);
        nvgLineTo(vg, state.high_beam ? 610 : 570, 285);
        nvgLineTo(vg, 545, 570);
        nvgClosePath(vg);
        fill_path(vg, nvgRGBA(255, 244, 180, state.high_beam ? 42 : 25));
    }

    const float car_x = 500 + static_cast<float>(state.lateral_position_m * 70);
    draw_car(vg, car_x, 555, static_cast<float>(state.steering));

    nvgBeginPath(vg);
    nvgRoundedRect(vg, 18, 18, 310, 68, 12);
    fill_path(vg, nvgRGBA(7, 13, 22, 190));
    char status[96];
    std::snprintf(status,
                  sizeof status,
                  "%.0f km/h   %.1f km",
                  state.speed_kph,
                  state.distance_m / 1000);
    text(vg, status, 34, 40, 22, nvgRGB(240, 246, 250));
    text(vg,
         "W/S throttle & brake   A/D steer   L/H lights",
         34,
         67,
         12,
         nvgRGB(160, 181, 198));

    if (std::abs(state.lateral_position_m) > 2.8) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 370, 24, 260, 42, 12);
        fill_path(vg, nvgRGBA(245, 180, 55, 210));
        text(vg,
             "LANE DEPARTURE",
             500,
             45,
             18,
             nvgRGB(30, 24, 12),
             NVG_ALIGN_CENTER);
    }
    if (state.battery_fault || state.tire_fault || state.drivetrain_fault) {
        text(vg,
             "FAULT ACTIVE",
             970,
             42,
             16,
             nvgRGB(255, 92, 92),
             NVG_ALIGN_RIGHT);
    }
    nvgRestore(vg);
}

void draw_simulator_hud(NVGcontext *vg,
                        const Rect &bounds,
                        const SimulatorVisualState &state) {
    constexpr float reference_width = 1000;
    constexpr float reference_height = 650;
    const float scale = std::min(bounds.width / reference_width,
                                 bounds.height / reference_height);
    const float origin_x =
        bounds.x + (bounds.width - reference_width * scale) / 2;
    const float origin_y =
        bounds.y + (bounds.height - reference_height * scale) / 2;
    nvgSave(vg);
    nvgTranslate(vg, origin_x, origin_y);
    nvgScale(vg, scale, scale);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 18, 18, 330, 68, 12);
    fill_path(vg, nvgRGBA(7, 13, 22, 205));
    char status[96];
    std::snprintf(status,
                  sizeof status,
                  "%.0f km/h   %.1f km",
                  state.speed_kph,
                  state.distance_m / 1000);
    text(vg, status, 34, 40, 22, nvgRGB(240, 246, 250));
    text(vg,
         "W/S accelerate & brake   A/D steer   L/H lights",
         34,
         67,
         12,
         nvgRGB(160, 181, 198));
    if (std::abs(state.lateral_position_m) > 2.8) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 370, 24, 260, 42, 12);
        fill_path(vg, nvgRGBA(245, 180, 55, 220));
        text(vg,
             "LANE DEPARTURE",
             500,
             45,
             18,
             nvgRGB(30, 24, 12),
             NVG_ALIGN_CENTER);
    }
    if (state.battery_fault || state.tire_fault || state.drivetrain_fault)
        text(vg,
             "FAULT ACTIVE",
             970,
             42,
             16,
             nvgRGB(255, 92, 92),
             NVG_ALIGN_RIGHT);
    nvgRestore(vg);
}
