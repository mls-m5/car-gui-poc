#pragma once

#include "dashboard.h"

#include <GLES2/gl2.h>

class DashboardGlowRenderer {
public:
    DashboardGlowRenderer() = default;
    ~DashboardGlowRenderer();
    DashboardGlowRenderer(const DashboardGlowRenderer &) = delete;
    DashboardGlowRenderer &operator=(const DashboardGlowRenderer &) = delete;

    bool initialize();
    void render(int pixel_width,
                int pixel_height,
                float logical_width,
                float logical_height,
                const Rect &bounds,
                const VehicleData &data,
                DriverDashboardStyle style,
                const DriverDashboardAnimation &animation);
    void shutdown();

private:
    bool resize(int width, int height);

    GLuint mask_program_ = 0;
    GLuint blur_program_ = 0;
    GLuint vertex_buffer_ = 0;
    GLuint framebuffers_[2]{};
    GLuint textures_[2]{};
    GLint mask_position_attribute_ = -1;
    GLint mask_color_attribute_ = -1;
    GLint blur_position_attribute_ = -1;
    GLint blur_uv_attribute_ = -1;
    GLint blur_texture_uniform_ = -1;
    GLint blur_direction_uniform_ = -1;
    int texture_width_ = 0;
    int texture_height_ = 0;
};
