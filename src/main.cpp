#include "dashboard.h"
#include <GLES2/gl2.h>
#include <SDL2/SDL.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
struct Display {
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    NVGcontext *vg = nullptr;
    bool open = false;
};
static void error(const char *s) {
    std::cerr << s << ": " << SDL_GetError() << '\n';
}
static bool create_display(Display &d,
                           const char *title,
                           int x,
                           int y,
                           int w,
                           int h,
                           const char *font_path) {
    d.window = SDL_CreateWindow(title,
                                x,
                                y,
                                w,
                                h,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                    SDL_WINDOW_ALLOW_HIGHDPI);
    if (!d.window) {
        error("SDL_CreateWindow");
        return false;
    }
    d.context = SDL_GL_CreateContext(d.window);
    if (!d.context) {
        error("SDL_GL_CreateContext");
        return false;
    }
    SDL_GL_MakeCurrent(d.window, d.context);
    d.vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!d.vg) {
        std::cerr << "nvgCreateGLES2 failed\n";
        return false;
    }
    if (nvgCreateFont(d.vg, "regular", font_path) < 0) {
        std::cerr << "Could not load font: " << font_path << '\n';
        return false;
    }
    d.open = true;
    return true;
}
static void destroy(Display &d) {
    if (d.vg) {
        SDL_GL_MakeCurrent(d.window, d.context);
        nvgDeleteGLES2(d.vg);
    }
    if (d.context)
        SDL_GL_DeleteContext(d.context);
    if (d.window)
        SDL_DestroyWindow(d.window);
    d = {};
}
static void render(Display &d, const VehicleData &data, bool driver) {
    if (!d.open)
        return;
    SDL_GL_MakeCurrent(d.window, d.context);
    int w, h, pw, ph;
    SDL_GetWindowSize(d.window, &w, &h);
    SDL_GL_GetDrawableSize(d.window, &pw, &ph);
    if (w <= 0 || h <= 0 || pw <= 0 || ph <= 0)
        return;
    glViewport(0, 0, pw, ph);
    glClearColor(.04f, .06f, .1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(d.vg, (float)w, (float)h, (float)pw / w);
    if (driver)
        draw_driver_display(d.vg, (float)w, (float)h, data);
    else
        draw_vehicle_details(d.vg, (float)w, (float)h, data);
    nvgEndFrame(d.vg);
    SDL_GL_SwapWindow(d.window);
}
int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        error("SDL_Init");
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "";
    std::string font = base + "assets/fonts/DejaVuSans.ttf";
    Display driver, details;
    if (!create_display(driver,
                        "EV Driver Display",
                        SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED,
                        1100,
                        600,
                        font.c_str()) ||
        !create_display(details,
                        "EV Vehicle Details",
                        SDL_WINDOWPOS_CENTERED + 80,
                        SDL_WINDOWPOS_CENTERED + 80,
                        850,
                        650,
                        font.c_str())) {
        destroy(details);
        destroy(driver);
        SDL_Quit();
        return 1;
    }
    SimulatedVehicleDataSource source;
    auto start = std::chrono::steady_clock::now();
    bool running = true;
    while (running && (driver.open || details.open)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                running = false;
            if (e.type == SDL_WINDOWEVENT &&
                e.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (e.window.windowID == SDL_GetWindowID(driver.window))
                    driver.open = false;
                if (e.window.windowID == SDL_GetWindowID(details.window))
                    details.open = false;
            }
        }
        double t = std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - start)
                       .count();
        VehicleData data = source.sample(t);
        if (driver.open)
            render(driver, data, true);
        if (details.open)
            render(details, data, false);
        SDL_Delay(16);
    }
    destroy(details);
    destroy(driver);
    SDL_Quit();
}
