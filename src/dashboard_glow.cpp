#include "dashboard_glow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

namespace {
constexpr float pi = 3.14159265f;
struct GlowVertex {
    float x, y;
    float r, g, b, a;
};
struct ScreenTransform {
    float logical_width;
    float logical_height;
    float scale;
    float offset_x;
    float offset_y;

    std::array<float, 2> point(float x, float y) const {
        const float screen_x = offset_x + x * scale;
        const float screen_y = offset_y + y * scale;
        return {screen_x / logical_width * 2.f - 1.f,
                1.f - screen_y / logical_height * 2.f};
    }
};
void vertex(std::vector<GlowVertex> &vertices,
            const ScreenTransform &transform,
            float x,
            float y,
            std::array<float, 3> color,
            float alpha) {
    const auto position = transform.point(x, y);
    vertices.push_back({position[0],
                        position[1],
                        color[0] * alpha,
                        color[1] * alpha,
                        color[2] * alpha,
                        alpha});
}
void filled_triangle(std::vector<GlowVertex> &vertices,
                     const ScreenTransform &transform,
                     std::array<float, 2> a,
                     std::array<float, 2> b,
                     std::array<float, 2> c,
                     std::array<float, 3> color,
                     float alpha) {
    vertex(vertices, transform, a[0], a[1], color, alpha);
    vertex(vertices, transform, b[0], b[1], color, alpha);
    vertex(vertices, transform, c[0], c[1], color, alpha);
}
void filled_quad(std::vector<GlowVertex> &vertices,
                 const ScreenTransform &transform,
                 std::array<float, 2> a,
                 std::array<float, 2> b,
                 std::array<float, 2> c,
                 std::array<float, 2> d,
                 std::array<float, 3> color,
                 float alpha) {
    filled_triangle(vertices, transform, a, b, c, color, alpha);
    filled_triangle(vertices, transform, a, c, d, color, alpha);
}
void line_segment(std::vector<GlowVertex> &vertices,
                  const ScreenTransform &transform,
                  float x0,
                  float y0,
                  float x1,
                  float y1,
                  float width,
                  std::array<float, 3> color,
                  float alpha) {
    const float dx = x1 - x0, dy = y1 - y0;
    const float length = std::max(.001f, std::sqrt(dx * dx + dy * dy));
    const float nx = -dy / length * width * .5f;
    const float ny = dx / length * width * .5f;
    filled_quad(vertices,
                transform,
                {x0 + nx, y0 + ny},
                {x1 + nx, y1 + ny},
                {x1 - nx, y1 - ny},
                {x0 - nx, y0 - ny},
                color,
                alpha);
}
template <std::size_t N>
void line_strip(std::vector<GlowVertex> &vertices,
                const ScreenTransform &transform,
                const std::array<std::array<float, 2>, N> &points,
                float width,
                std::array<float, 3> color,
                float alpha,
                bool closed = false) {
    for (std::size_t i = 1; i < points.size(); ++i)
        line_segment(vertices,
                     transform,
                     points[i - 1][0],
                     points[i - 1][1],
                     points[i][0],
                     points[i][1],
                     width,
                     color,
                     alpha);
    if (closed)
        line_segment(vertices,
                     transform,
                     points.back()[0],
                     points.back()[1],
                     points.front()[0],
                     points.front()[1],
                     width,
                     color,
                     alpha);
}
void arc(std::vector<GlowVertex> &vertices,
         const ScreenTransform &transform,
         float x,
         float y,
         float radius,
         float width,
         float percentage,
         bool clockwise,
         std::array<float, 3> color,
         float alpha) {
    percentage = std::clamp(percentage, 0.f, 1.f);
    if (percentage <= .001f)
        return;
    const int segments = std::max(1, (int)std::ceil(percentage * 64));
    const float direction = clockwise ? 1.f : -1.f;
    for (int i = 0; i < segments; ++i) {
        const float p0 = percentage * i / segments;
        const float p1 = percentage * (i + 1) / segments;
        const float a0 = -pi / 2 + direction * p0 * 2.f * pi;
        const float a1 = -pi / 2 + direction * p1 * 2.f * pi;
        const float inner = radius - width / 2;
        const float outer = radius + width / 2;
        const float x0i = x + std::cos(a0) * inner;
        const float y0i = y + std::sin(a0) * inner;
        const float x0o = x + std::cos(a0) * outer;
        const float y0o = y + std::sin(a0) * outer;
        const float x1i = x + std::cos(a1) * inner;
        const float y1i = y + std::sin(a1) * inner;
        const float x1o = x + std::cos(a1) * outer;
        const float y1o = y + std::sin(a1) * outer;
        vertex(vertices, transform, x0i, y0i, color, alpha);
        vertex(vertices, transform, x0o, y0o, color, alpha);
        vertex(vertices, transform, x1o, y1o, color, alpha);
        vertex(vertices, transform, x0i, y0i, color, alpha);
        vertex(vertices, transform, x1o, y1o, color, alpha);
        vertex(vertices, transform, x1i, y1i, color, alpha);
    }
}
void indicator_mask(std::vector<GlowVertex> &vertices,
                    const ScreenTransform &transform,
                    int icon,
                    float x,
                    float y,
                    std::array<float, 3> color,
                    float alpha) {
    if (icon == 0 || icon == 7) {
        const float mirror = icon == 0 ? 1.f : -1.f;
        filled_triangle(vertices,
                        transform,
                        {x - mirror * 15, y},
                        {x - mirror * 1, y - 11},
                        {x - mirror * 1, y + 11},
                        color,
                        alpha);
        filled_quad(vertices,
                    transform,
                    {x - mirror * 2, y - 4},
                    {x + mirror * 14, y - 4},
                    {x + mirror * 14, y + 4},
                    {x - mirror * 2, y + 4},
                    color,
                    alpha);
        return;
    }
    if (icon == 1 || icon == 2) {
        const std::array<std::array<float, 2>, 7> lamp = {{{x - 12, y - 10},
                                                           {x - 7, y - 9},
                                                           {x - 3, y - 5},
                                                           {x - 2, y},
                                                           {x - 3, y + 5},
                                                           {x - 7, y + 9},
                                                           {x - 12, y + 10}}};
        line_strip(vertices, transform, lamp, 2.6f, color, alpha);
        for (int line = -1; line <= 1; ++line)
            line_segment(vertices,
                         transform,
                         x + 1,
                         y + line * 7,
                         x + 14,
                         y + line * 7 + (icon == 1 ? 3 : 0),
                         2.6f,
                         color,
                         alpha);
        return;
    }
    if (icon == 3) {
        line_segment(vertices,
                     transform,
                     x - 12,
                     y + 12,
                     x - 7,
                     y - 12,
                     2.6f,
                     color,
                     alpha);
        line_segment(vertices,
                     transform,
                     x + 12,
                     y + 12,
                     x + 7,
                     y - 12,
                     2.6f,
                     color,
                     alpha);
        line_segment(
            vertices, transform, x, y + 9, x, y - 8, 2.6f, color, alpha);
        line_segment(
            vertices, transform, x - 4, y - 3, x, y - 8, 2.6f, color, alpha);
        line_segment(
            vertices, transform, x, y - 8, x + 4, y - 3, 2.6f, color, alpha);
        return;
    }
    if (icon == 4) {
        arc(vertices, transform, x, y + 1, 12, 2.8f, .62f, true, color, alpha);
        line_segment(
            vertices, transform, x, y + 1, x + 7, y - 5, 2.8f, color, alpha);
        return;
    }
    if (icon == 5) {
        const std::array<std::array<float, 2>, 8> tire = {{{x - 12, y - 10},
                                                           {x - 15, y},
                                                           {x - 10, y + 10},
                                                           {x - 5, y + 12},
                                                           {x + 5, y + 12},
                                                           {x + 10, y + 10},
                                                           {x + 15, y},
                                                           {x + 12, y - 10}}};
        line_strip(vertices, transform, tire, 2.8f, color, alpha);
        line_segment(
            vertices, transform, x, y - 5, x, y + 4, 2.8f, color, alpha);
        line_segment(
            vertices, transform, x, y + 8, x, y + 10, 2.8f, color, alpha);
        return;
    }
    const std::array<std::array<float, 2>, 3> warning = {
        {{x, y - 13}, {x - 14, y + 12}, {x + 14, y + 12}}};
    line_strip(vertices, transform, warning, 2.8f, color, alpha, true);
    line_segment(vertices, transform, x, y - 5, x, y + 4, 2.8f, color, alpha);
    line_segment(vertices, transform, x, y + 8, x, y + 10, 2.8f, color, alpha);
}
GLuint compile_shader(GLenum type, const char *source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        std::cerr << "Dashboard glow shader compilation failed: " << log
                  << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
GLuint create_program(const char *vertex_source, const char *fragment_source) {
    const GLuint vertex_shader =
        compile_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment_shader =
        compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex_shader || !fragment_shader) {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024]{};
        glGetProgramInfoLog(program, sizeof log, nullptr, log);
        std::cerr << "Dashboard glow shader link failed: " << log << '\n';
        glDeleteProgram(program);
        return 0;
    }
    return program;
}
} // namespace

DashboardGlowRenderer::~DashboardGlowRenderer() {
    shutdown();
}

bool DashboardGlowRenderer::initialize() {
    const char *mask_vertex =
        "attribute vec2 aPosition; attribute vec4 aColor; varying vec4 vColor;"
        "void main(){vColor=aColor;gl_Position=vec4(aPosition,0.0,1.0);}";
    const char *mask_fragment = "precision mediump float;varying vec4 vColor;"
                                "void main(){gl_FragColor=vColor;}";
    const char *blur_vertex =
        "attribute vec2 aPosition;attribute vec2 aUv;varying vec2 vUv;"
        "void main(){vUv=aUv;gl_Position=vec4(aPosition,0.0,1.0);}";
    const char *blur_fragment =
        "precision mediump float;varying vec2 vUv;uniform sampler2D uTexture;"
        "uniform vec2 uDirection;void main(){vec4 "
        "c=texture2D(uTexture,vUv)*0.227027;"
        "c+=texture2D(uTexture,vUv+uDirection*1.384615)*0.316216;"
        "c+=texture2D(uTexture,vUv-uDirection*1.384615)*0.316216;"
        "c+=texture2D(uTexture,vUv+uDirection*3.230769)*0.070270;"
        "c+=texture2D(uTexture,vUv-uDirection*3.230769)*0.070270;"
        "gl_FragColor=c;}";
    mask_program_ = create_program(mask_vertex, mask_fragment);
    blur_program_ = create_program(blur_vertex, blur_fragment);
    if (!mask_program_ || !blur_program_) {
        shutdown();
        return false;
    }
    mask_position_attribute_ = glGetAttribLocation(mask_program_, "aPosition");
    mask_color_attribute_ = glGetAttribLocation(mask_program_, "aColor");
    blur_position_attribute_ = glGetAttribLocation(blur_program_, "aPosition");
    blur_uv_attribute_ = glGetAttribLocation(blur_program_, "aUv");
    blur_texture_uniform_ = glGetUniformLocation(blur_program_, "uTexture");
    blur_direction_uniform_ = glGetUniformLocation(blur_program_, "uDirection");
    glGenBuffers(1, &vertex_buffer_);
    glGenFramebuffers(2, framebuffers_);
    glGenTextures(2, textures_);
    return vertex_buffer_ && framebuffers_[0] && framebuffers_[1] &&
           textures_[0] && textures_[1];
}

bool DashboardGlowRenderer::resize(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (width == texture_width_ && height == texture_height_)
        return true;
    texture_width_ = width;
    texture_height_ = height;
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     texture_width_,
                     texture_height_,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               textures_[i],
                               0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void DashboardGlowRenderer::render(int pixel_width,
                                   int pixel_height,
                                   float logical_width,
                                   float logical_height,
                                   const Rect &bounds,
                                   const VehicleData &data,
                                   DriverDashboardStyle style,
                                   const DriverDashboardAnimation &animation) {
    if (!mask_program_ || style != DriverDashboardStyle::modern_rings ||
        pixel_width <= 0 || pixel_height <= 0 || logical_width <= 0 ||
        logical_height <= 0 || !resize(pixel_width, pixel_height))
        return;

    const float scale = std::min(bounds.width / 800.f, bounds.height / 480.f);
    const ScreenTransform transform{logical_width,
                                    logical_height,
                                    scale,
                                    bounds.x + (bounds.width - 800 * scale) / 2,
                                    bounds.y +
                                        (bounds.height - 480 * scale) / 2};
    std::vector<GlowVertex> vertices;
    vertices.reserve(1200);
    const std::array<std::array<float, 3>, 8> colors = {
        std::array<float, 3>{.063f, .725f, .506f},
        std::array<float, 3>{.063f, .725f, .506f},
        std::array<float, 3>{.231f, .51f, .965f},
        std::array<float, 3>{.063f, .725f, .506f},
        std::array<float, 3>{0, .745f, .961f},
        std::array<float, 3>{.961f, .62f, .043f},
        std::array<float, 3>{.937f, .267f, .267f},
        std::array<float, 3>{.063f, .725f, .506f}};
    const float icon_x[] = {204, 260, 316, 372, 428, 484, 540, 596};
    for (int i = 0; i < 8; ++i) {
        const float intensity = animation.indicator_intensity[i];
        if (intensity > .001f)
            indicator_mask(vertices,
                           transform,
                           i,
                           icon_x[i],
                           58,
                           colors[i],
                           intensity * .75f);
    }
    arc(vertices,
        transform,
        225,
        225,
        90,
        9,
        (float)data.speed_kph / 220.f,
        false,
        {0, .949f, .996f},
        .55f);
    const double displayed_power = animation.displayed_power_kw;
    const bool regenerating = displayed_power < -.05;
    const float power_limit =
        (float)(regenerating ? data.available_regen_power_kw
                             : data.available_discharge_power_kw);
    arc(vertices,
        transform,
        575,
        225,
        90,
        9,
        (float)std::abs(displayed_power) / (std::max(1.f, power_limit) * 4.f),
        !regenerating,
        regenerating ? std::array<float, 3>{.937f, .267f, .267f}
                     : std::array<float, 3>{0, .949f, .996f},
        .6f);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[0]);
    glViewport(0, 0, texture_width_, texture_height_);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(mask_program_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(GlowVertex),
                 vertices.data(),
                 GL_STREAM_DRAW);
    glEnableVertexAttribArray(mask_position_attribute_);
    glEnableVertexAttribArray(mask_color_attribute_);
    glVertexAttribPointer(mask_position_attribute_,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(GlowVertex),
                          (void *)offsetof(GlowVertex, x));
    glVertexAttribPointer(mask_color_attribute_,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(GlowVertex),
                          (void *)offsetof(GlowVertex, r));
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
    glDisableVertexAttribArray(mask_position_attribute_);
    glDisableVertexAttribArray(mask_color_attribute_);

    const float quad[] = {-1, -1, 0, 0, 1, -1, 1, 0, -1, 1, 0, 1,
                          -1, 1,  0, 1, 1, -1, 1, 0, 1,  1, 1, 1};
    glUseProgram(blur_program_);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STREAM_DRAW);
    glEnableVertexAttribArray(blur_position_attribute_);
    glEnableVertexAttribArray(blur_uv_attribute_);
    glVertexAttribPointer(blur_position_attribute_,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          4 * sizeof(float),
                          nullptr);
    glVertexAttribPointer(blur_uv_attribute_,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          4 * sizeof(float),
                          (void *)(2 * sizeof(float)));
    glUniform1i(blur_texture_uniform_, 0);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[1]);
    glBindTexture(GL_TEXTURE_2D, textures_[0]);
    glUniform2f(blur_direction_uniform_, 2.f / texture_width_, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[0]);
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glUniform2f(blur_direction_uniform_, 0, 2.f / texture_height_);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, pixel_width, pixel_height);
    glBindTexture(GL_TEXTURE_2D, textures_[0]);
    glUniform2f(blur_direction_uniform_, 0, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(blur_position_attribute_);
    glDisableVertexAttribArray(blur_uv_attribute_);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void DashboardGlowRenderer::shutdown() {
    if (textures_[0] || textures_[1])
        glDeleteTextures(2, textures_);
    if (framebuffers_[0] || framebuffers_[1])
        glDeleteFramebuffers(2, framebuffers_);
    if (vertex_buffer_)
        glDeleteBuffers(1, &vertex_buffer_);
    if (mask_program_)
        glDeleteProgram(mask_program_);
    if (blur_program_)
        glDeleteProgram(blur_program_);
    textures_[0] = textures_[1] = 0;
    framebuffers_[0] = framebuffers_[1] = 0;
    vertex_buffer_ = mask_program_ = blur_program_ = 0;
    texture_width_ = texture_height_ = 0;
}
