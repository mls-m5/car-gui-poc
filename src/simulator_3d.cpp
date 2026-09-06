#include "simulator_3d.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

namespace {
struct Vec3 {
    float x, y, z;
};
struct Vertex {
    Vec3 position;
    std::array<float, 3> color;
};
struct Mat4 {
    float v[16]{};
};
Vec3 subtract(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 normalize(Vec3 a) {
    float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return {a.x / l, a.y / l, a.z / l};
}
float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Mat4 multiply(const Mat4 &a, const Mat4 &b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                r.v[c * 4 + row] += a.v[k * 4 + row] * b.v[c * 4 + k];
    return r;
}
Mat4 perspective(float fov, float aspect, float near_plane, float far_plane) {
    Mat4 m;
    float f = 1 / std::tan(fov * .5f);
    m.v[0] = f / aspect;
    m.v[5] = f;
    m.v[10] = (far_plane + near_plane) / (near_plane - far_plane);
    m.v[11] = -1;
    m.v[14] = (2 * far_plane * near_plane) / (near_plane - far_plane);
    return m;
}
Mat4 look_at(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = normalize(subtract(center, eye)), s = normalize(cross(f, up)),
         u = cross(s, f);
    Mat4 m;
    m.v[0] = s.x;
    m.v[4] = s.y;
    m.v[8] = s.z;
    m.v[1] = u.x;
    m.v[5] = u.y;
    m.v[9] = u.z;
    m.v[2] = -f.x;
    m.v[6] = -f.y;
    m.v[10] = -f.z;
    m.v[15] = 1;
    m.v[12] = -dot(s, eye);
    m.v[13] = -dot(u, eye);
    m.v[14] = dot(f, eye);
    return m;
}
void triangle(std::vector<Vertex> &v,
              Vec3 a,
              Vec3 b,
              Vec3 c,
              std::array<float, 3> color) {
    v.push_back({a, color});
    v.push_back({b, color});
    v.push_back({c, color});
}
void quad(std::vector<Vertex> &v,
          Vec3 a,
          Vec3 b,
          Vec3 c,
          Vec3 d,
          std::array<float, 3> color) {
    triangle(v, a, b, c, color);
    triangle(v, a, c, d, color);
}
void oriented_box(std::vector<Vertex> &v,
                  Vec3 center,
                  Vec3 size,
                  float yaw,
                  std::array<float, 3> color) {
    Vec3 p[8];
    const float sine = std::sin(yaw);
    const float cosine = std::cos(yaw);
    for (int i = 0; i < 8; ++i) {
        const float local_x = ((i & 1) ? .5f : -.5f) * size.x;
        const float local_z = ((i & 4) ? .5f : -.5f) * size.z;
        p[i] = {center.x + cosine * local_x + sine * local_z,
                center.y + ((i & 2) ? .5f : -.5f) * size.y,
                center.z - sine * local_x + cosine * local_z};
    }
    quad(v, p[0], p[4], p[6], p[2], color);
    quad(v, p[1], p[3], p[7], p[5], color);
    quad(v, p[2], p[6], p[7], p[3], color);
    quad(v, p[0], p[1], p[5], p[4], color);
    quad(v, p[4], p[5], p[7], p[6], color);
    quad(v, p[0], p[2], p[3], p[1], color);
}
void box(std::vector<Vertex> &v,
         Vec3 center,
         Vec3 size,
         std::array<float, 3> color) {
    oriented_box(v, center, size, 0, color);
}
void wheel(std::vector<Vertex> &v,
           Vec3 center,
           float radius,
           float width,
           float yaw,
           std::array<float, 3> color) {
    constexpr int sides = 12;
    const float sine = std::sin(yaw), cosine = std::cos(yaw);
    for (int i = 0; i < sides; ++i) {
        const float a0 = i * 2.f * 3.14159265f / sides;
        const float a1 = (i + 1) * 2.f * 3.14159265f / sides;
        auto point = [&](float axle, float angle) {
            const float local_y = std::cos(angle) * radius;
            const float local_z = std::sin(angle) * radius;
            return Vec3{center.x + cosine * axle + sine * local_z,
                        center.y + local_y,
                        center.z - sine * axle + cosine * local_z};
        };
        quad(v,
             point(-width / 2, a0),
             point(width / 2, a0),
             point(width / 2, a1),
             point(-width / 2, a1),
             color);
    }
}
void pine(std::vector<Vertex> &v, float x, float z, float scale, bool night) {
    box(v,
        {x, .7f * scale, z},
        {.18f * scale, 1.4f * scale, .18f * scale},
        {.28f, .16f, .07f});
    auto color = night ? std::array<float, 3>{.03f, .2f, .13f}
                       : std::array<float, 3>{.03f, .42f, .19f};
    for (int layer = 0; layer < 3; ++layer) {
        float y = (1.1f + layer * .55f) * scale,
              r = (1.2f - layer * .18f) * scale;
        Vec3 top{x, (2.7f + layer * .25f) * scale, z};
        triangle(v, {x - r, y, z - r}, top, {x + r, y, z - r}, color);
        triangle(v, {x + r, y, z - r}, top, {x + r, y, z + r}, color);
        triangle(v, {x + r, y, z + r}, top, {x - r, y, z + r}, color);
        triangle(v, {x - r, y, z + r}, top, {x - r, y, z - r}, color);
    }
}
GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        std::cerr << "3D shader compilation failed: " << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
} // namespace

Simulator3DRenderer::~Simulator3DRenderer() {
    shutdown();
}
bool Simulator3DRenderer::initialize() {
    const char *vertex_source =
        "attribute vec3 aPosition; attribute vec3 aColor; uniform mat4 "
        "uViewProjection; varying vec3 vColor; varying vec3 vWorldPosition; "
        "void main(){vColor=aColor;vWorldPosition=aPosition;"
        "gl_Position=uViewProjection*vec4(aPosition,1.0);}";
    const char *fragment_source =
        "precision mediump float; varying vec3 vColor; varying vec3 "
        "vWorldPosition; uniform float uHeadlights; uniform float uHighBeam; "
        "uniform vec3 uCarPosition; uniform vec3 uCarForward; void main(){"
        "vec3 toFragment=vWorldPosition-uCarPosition; float "
        "distance=length(toFragment);"
        "float cone=dot(normalize(toFragment),normalize(uCarForward));"
        "float cutoff=mix(0.78,0.90,uHighBeam); float "
        "range=mix(32.0,75.0,uHighBeam);"
        "float "
        "beam=smoothstep(cutoff,1.0,cone)*(1.0-smoothstep(0.0,range,distance));"
        "vec3 result=vColor+vec3(0.8,0.68,0.3)*beam*uHeadlights;"
        "gl_FragColor=vec4(result,1.0);}";
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source),
           fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex || !fragment)
        return false;
    program_ = glCreateProgram();
    glAttachShader(program_, vertex);
    glAttachShader(program_, fragment);
    glBindAttribLocation(program_, 0, "aPosition");
    glBindAttribLocation(program_, 1, "aColor");
    glLinkProgram(program_);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        std::cerr << "3D shader link failed\n";
        shutdown();
        return false;
    }
    position_attribute_ = glGetAttribLocation(program_, "aPosition");
    color_attribute_ = glGetAttribLocation(program_, "aColor");
    view_projection_uniform_ =
        glGetUniformLocation(program_, "uViewProjection");
    headlights_uniform_ = glGetUniformLocation(program_, "uHeadlights");
    high_beam_uniform_ = glGetUniformLocation(program_, "uHighBeam");
    car_position_uniform_ = glGetUniformLocation(program_, "uCarPosition");
    car_forward_uniform_ = glGetUniformLocation(program_, "uCarForward");
    glGenBuffers(1, &vertex_buffer_);
    return vertex_buffer_ != 0;
}
void Simulator3DRenderer::shutdown() {
    if (vertex_buffer_)
        glDeleteBuffers(1, &vertex_buffer_);
    if (program_)
        glDeleteProgram(program_);
    vertex_buffer_ = 0;
    program_ = 0;
}
void Simulator3DRenderer::render(int viewport_x,
                                 int viewport_y,
                                 int viewport_width,
                                 int viewport_height,
                                 const SimulatorVisualState &s) {
    if (!program_ || viewport_width <= 0 || viewport_height <= 0)
        return;
    bool night = std::fmod(s.distance_m, 1400.0) > 950;
    if (night)
        glClearColor(0.025f, 0.055f, 0.12f, 1.f);
    else
        glClearColor(0.34f, 0.68f, 0.87f, 1.f);
    glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(viewport_x, viewport_y, viewport_width, viewport_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    std::vector<Vertex> vertices;
    vertices.reserve(9000);
    const float car_x = (float)s.world_position_x_m;
    const float car_z = (float)s.world_position_z_m;
    quad(vertices,
         {car_x - 350, 0, car_z - 350},
         {car_x + 350, 0, car_z - 350},
         {car_x + 350, 0, car_z + 350},
         {car_x - 350, 0, car_z + 350},
         night ? std::array<float, 3>{.02f, .12f, .09f}
               : std::array<float, 3>{.12f, .38f, .16f});
    constexpr float world_end = 10000;
    quad(vertices,
         {-5, .015f, -world_end},
         {5, .015f, -world_end},
         {5, .015f, world_end},
         {-5, .015f, world_end},
         {.16f, .17f, .19f});
    quad(vertices,
         {-5.05f, .025f, -world_end},
         {-4.85f, .025f, -world_end},
         {-4.85f, .025f, world_end},
         {-5.05f, .025f, world_end},
         {.85f, .85f, .76f});
    quad(vertices,
         {4.85f, .025f, -world_end},
         {5.05f, .025f, -world_end},
         {5.05f, .025f, world_end},
         {4.85f, .025f, world_end},
         {.85f, .85f, .76f});
    const int first_stripe = (int)std::floor((car_z - 180) / 18.f);
    for (int i = first_stripe; i < first_stripe + 30; ++i) {
        const float z = i * 18.f;
        quad(vertices,
             {-.09f, .03f, z},
             {.09f, .03f, z},
             {.09f, .03f, z + 8},
             {-.09f, .03f, z + 8},
             {.9f, .82f, .32f});
    }
    const int first_tree = (int)std::floor((car_z - 180) / 24.f);
    for (int i = first_tree; i < first_tree + 30; ++i) {
        const float z = i * 24.f;
        const int pattern = ((i % 7) + 7) % 7;
        const float variation = pattern * .72f;
        pine(vertices,
             -8.5f - variation,
             z,
             1.05f + (pattern % 3) * .18f,
             night);
        pine(vertices,
             8.5f + variation,
             z + 9,
             1.0f + ((pattern + 1) % 3) * .2f,
             night);
    }
    const int first_mountain = (int)std::floor((car_z - 250) / 90.f);
    for (int i = first_mountain; i < first_mountain + 10; ++i) {
        const float z = i * 90.f + 45;
        const float x = (i % 2 ? -1.f : 1.f) * (30 + (std::abs(i) % 4) * 8);
        const float mountain_scale = 8 + (std::abs(i) % 3) * 2;
        const Vec3 top{x, mountain_scale * 2.6f, z};
        triangle(vertices,
                 {x - mountain_scale, 0, z - mountain_scale},
                 top,
                 {x + mountain_scale, 0, z - mountain_scale},
                 night ? std::array<float, 3>{.1f, .14f, .18f}
                       : std::array<float, 3>{.28f, .34f, .36f});
        triangle(vertices,
                 {x + mountain_scale, 0, z - mountain_scale},
                 top,
                 {x, 0, z + mountain_scale},
                 night ? std::array<float, 3>{.08f, .12f, .16f}
                       : std::array<float, 3>{.22f, .3f, .32f});
    }
    const float car_yaw = (float)s.heading_radians;
    const float yaw_sine = std::sin(car_yaw);
    const float yaw_cosine = std::cos(car_yaw);
    const auto car_point = [&](float local_x, float y, float local_z) {
        return Vec3{car_x + yaw_cosine * local_x + yaw_sine * local_z,
                    y,
                    car_z - yaw_sine * local_x + yaw_cosine * local_z};
    };
    oriented_box(vertices,
                 {car_x, .48f, 2},
                 {1.9f, .65f, 4.2f},
                 car_yaw,
                 {.04f, .45f, .75f});
    oriented_box(vertices,
                 car_point(0, .93f, .15f),
                 {1.45f, .42f, 2.1f},
                 car_yaw,
                 {.04f, .12f, .18f});
    oriented_box(vertices,
                 car_point(-.58f, .54f, -2.13f),
                 {.38f, .18f, .06f},
                 car_yaw,
                 {.95f, .05f, .04f});
    oriented_box(vertices,
                 car_point(.58f, .54f, -2.13f),
                 {.38f, .18f, .06f},
                 car_yaw,
                 {.95f, .05f, .04f});
    const std::array<float, 3> tire_color{.025f, .03f, .035f};
    for (float side : {-1.f, 1.f}) {
        wheel(vertices,
              car_point(side * 1.02f, .42f, -1.25f),
              .38f,
              .34f,
              car_yaw,
              tire_color);
        wheel(vertices,
              car_point(side * 1.02f, .42f, 1.25f),
              .38f,
              .34f,
              car_yaw + (float)s.steering * .48f,
              tire_color);
    }
    if (s.headlights && night) {
        auto light = s.high_beam ? std::array<float, 3>{.65f, .62f, .35f}
                                 : std::array<float, 3>{.38f, .37f, .22f};
        const float reach = s.high_beam ? 43.f : 23.f;
        triangle(vertices,
                 car_point(-.65f, .05f, 2.05f),
                 car_point(-4, .05f, reach),
                 car_point(0, .05f, reach),
                 light);
        triangle(vertices,
                 car_point(.65f, .05f, 2.05f),
                 car_point(0, .05f, reach),
                 car_point(4, .05f, reach),
                 light);
    }
    const Vec3 forward{yaw_sine, 0, yaw_cosine};
    const Vec3 car_position{car_x, .8f, car_z};
    const Vec3 camera_position{
        car_x - forward.x * 10.f, 4.5f, car_z - forward.z * 10.f};
    const Vec3 camera_target{
        car_x + forward.x * 18.f, 1.0f, car_z + forward.z * 18.f};
    Mat4 projection = perspective(60.f * 3.14159265f / 180.f,
                                  (float)viewport_width / viewport_height,
                                  .1f,
                                  350.f),
         view = look_at(camera_position, camera_target, {0, 1, 0}),
         vp = multiply(projection, view);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glUseProgram(program_);
    glUniformMatrix4fv(view_projection_uniform_, 1, GL_FALSE, vp.v);
    glUniform1f(headlights_uniform_, s.headlights ? 1.f : 0.f);
    glUniform1f(high_beam_uniform_, s.high_beam ? 1.f : 0.f);
    glUniform3f(
        car_position_uniform_, car_position.x, car_position.y, car_position.z);
    glUniform3f(car_forward_uniform_, forward.x, forward.y, forward.z);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(Vertex),
                 vertices.data(),
                 GL_STREAM_DRAW);
    glEnableVertexAttribArray(position_attribute_);
    glEnableVertexAttribArray(color_attribute_);
    glVertexAttribPointer(position_attribute_,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          (void *)offsetof(Vertex, position));
    glVertexAttribPointer(color_attribute_,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Vertex),
                          (void *)offsetof(Vertex, color));
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
    glDisableVertexAttribArray(position_attribute_);
    glDisableVertexAttribArray(color_attribute_);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
}
