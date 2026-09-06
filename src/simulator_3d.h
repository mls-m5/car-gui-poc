#pragma once

#include "interactive_vehicle_simulator.h"

#include <GLES2/gl2.h>

class Simulator3DRenderer {
public:
    Simulator3DRenderer() = default;
    ~Simulator3DRenderer();
    Simulator3DRenderer(const Simulator3DRenderer &) = delete;
    Simulator3DRenderer &operator=(const Simulator3DRenderer &) = delete;

    bool initialize();
    void render(int width, int height, const SimulatorVisualState &state);
    void shutdown();

private:
    GLuint program_ = 0;
    GLuint vertex_buffer_ = 0;
    GLint position_attribute_ = -1;
    GLint color_attribute_ = -1;
    GLint view_projection_uniform_ = -1;
};
