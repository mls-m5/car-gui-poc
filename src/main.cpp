#include "nanovg_gles2.h"

#include <SDL2/SDL.h>

#include <GLES2/gl2.h>

#include <cmath>
#include <iostream>

namespace {

struct Application {
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    NVGcontext* vg = nullptr;
    bool running = true;
};

void report_sdl_error(const char* operation)
{
    std::cerr << operation << ": " << SDL_GetError() << '\n';
}

bool initialize(Application& app)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        report_sdl_error("SDL_Init");
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    app.window = SDL_CreateWindow(
        "NanoVG triangle",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800,
        600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (app.window == nullptr) {
        report_sdl_error("SDL_CreateWindow");
        return false;
    }

    app.gl_context = SDL_GL_CreateContext(app.window);
    if (app.gl_context == nullptr) {
        report_sdl_error("SDL_GL_CreateContext");
        return false;
    }

    if (SDL_GL_SetSwapInterval(1) != 0) {
        // Vsync is optional and is not available on every platform.
        std::cerr << "SDL_GL_SetSwapInterval: " << SDL_GetError() << '\n';
    }

    app.vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (app.vg == nullptr) {
        std::cerr << "nvgCreateGLES2 failed\n";
        return false;
    }

    return true;
}

void process_events(Application& app)
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT ||
            (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
            app.running = false;
        }
    }
}

void frame(Application& app)
{
    process_events(app);
    if (!app.running) {
        return;
    }

    int width = 0;
    int height = 0;
    int drawable_width = 0;
    int drawable_height = 0;
    SDL_GetWindowSize(app.window, &width, &height);
    SDL_GL_GetDrawableSize(app.window, &drawable_width, &drawable_height);

    if (width <= 0 || height <= 0 || drawable_width <= 0 || drawable_height <= 0) {
        return;
    }

    const float pixel_ratio = static_cast<float>(drawable_width) /
                              static_cast<float>(width);

    glViewport(0, 0, drawable_width, drawable_height);
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(app.vg, static_cast<float>(width), static_cast<float>(height), pixel_ratio);

    const float center_x = static_cast<float>(width) * 0.5f;
    const float center_y = static_cast<float>(height) * 0.5f;
    const float triangle_width = static_cast<float>(width) * 0.42f;
    const float triangle_height = static_cast<float>(height) * 0.48f;

    nvgBeginPath(app.vg);
    nvgMoveTo(app.vg, center_x, center_y - triangle_height * 0.5f);
    nvgLineTo(app.vg, center_x - triangle_width * 0.5f, center_y + triangle_height * 0.5f);
    nvgLineTo(app.vg, center_x + triangle_width * 0.5f, center_y + triangle_height * 0.5f);
    nvgClosePath(app.vg);
    nvgFillColor(app.vg, nvgRGBA(55, 170, 255, 255));
    nvgFill(app.vg);

    nvgBeginPath(app.vg);
    nvgMoveTo(app.vg, center_x, center_y - triangle_height * 0.5f);
    nvgLineTo(app.vg, center_x - triangle_width * 0.5f, center_y + triangle_height * 0.5f);
    nvgLineTo(app.vg, center_x + triangle_width * 0.5f, center_y + triangle_height * 0.5f);
    nvgClosePath(app.vg);
    nvgStrokeColor(app.vg, nvgRGBA(225, 245, 255, 255));
    nvgStrokeWidth(app.vg, 3.0f);
    nvgStroke(app.vg);

    nvgEndFrame(app.vg);
    SDL_GL_SwapWindow(app.window);
}

void shutdown(Application& app)
{
    if (app.vg != nullptr) {
        nvgDeleteGLES2(app.vg);
        app.vg = nullptr;
    }
    if (app.gl_context != nullptr) {
        SDL_GL_DeleteContext(app.gl_context);
        app.gl_context = nullptr;
    }
    if (app.window != nullptr) {
        SDL_DestroyWindow(app.window);
        app.window = nullptr;
    }
    SDL_Quit();
}

} // namespace

int main()
{
    Application app;
    if (!initialize(app)) {
        shutdown(app);
        return 1;
    }

    while (app.running) {
        frame(app);
    }

    shutdown(app);
    return 0;
}
