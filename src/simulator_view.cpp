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
} // namespace

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
    nvgRoundedRect(vg, 18, 18, 390, 68, 12);
    fill_path(vg, nvgRGBA(7, 13, 22, 205));
    char status[96];
    std::snprintf(status,
                  sizeof status,
                  "%.0f km/h   %.1f km",
                  state.speed_kph,
                  state.distance_m / 1000);
    text(vg, status, 34, 40, 22, nvgRGB(240, 246, 250));
    text(vg,
         "W/S drive   A/D steer   K lane   +/- cruise   C pause",
         34,
         67,
         12,
         nvgRGB(160, 181, 198));
    if (std::abs(state.lateral_position_m) > 2.8) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 420, 24, 260, 42, 12);
        fill_path(vg, nvgRGBA(245, 180, 55, 220));
        text(vg,
             "LANE DEPARTURE",
             550,
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
