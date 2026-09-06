#include "dashboard.h"
#include <GLES2/gl2.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

struct Display {
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    NVGcontext *vg = nullptr;
    bool open = false;
};

static void error(const char *message) {
    std::cerr << message << ": " << SDL_GetError() << '\n';
}

static bool create_display(Display &display,
                           const char *title,
                           int x,
                           int y,
                           int width,
                           int height,
                           const char *font_path) {
    display.window = SDL_CreateWindow(title,
                                      x,
                                      y,
                                      width,
                                      height,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                          SDL_WINDOW_ALLOW_HIGHDPI);
    if (!display.window) {
        error("SDL_CreateWindow");
        return false;
    }
    display.context = SDL_GL_CreateContext(display.window);
    if (!display.context) {
        error("SDL_GL_CreateContext");
        return false;
    }
    if (SDL_GL_MakeCurrent(display.window, display.context) != 0) {
        error("SDL_GL_MakeCurrent");
        return false;
    }
    display.vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (!display.vg) {
        std::cerr << "nvgCreateGLES2 failed\n";
        return false;
    }
    if (nvgCreateFont(display.vg, "regular", font_path) < 0) {
        std::cerr << "Could not load font: " << font_path << '\n';
        return false;
    }
    display.open = true;
    return true;
}

static void destroy(Display &display) {
    if (display.vg) {
        SDL_GL_MakeCurrent(display.window, display.context);
        nvgDeleteGLES2(display.vg);
    }
    if (display.context)
        SDL_GL_DeleteContext(display.context);
    if (display.window)
        SDL_DestroyWindow(display.window);
    display = {};
}

static void render(Display &display,
                   const VehicleData &data,
                   const Rect &bounds,
                   bool driver) {
    if (!display.open ||
        SDL_GL_MakeCurrent(display.window, display.context) != 0)
        return;
    int width = 0, height = 0, pixel_width = 0, pixel_height = 0;
    SDL_GetWindowSize(display.window, &width, &height);
    SDL_GL_GetDrawableSize(display.window, &pixel_width, &pixel_height);
    if (width <= 0 || height <= 0 || pixel_width <= 0 || pixel_height <= 0)
        return;
    glViewport(0, 0, pixel_width, pixel_height);
    glClearColor(.04f, .06f, .1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(
        display.vg, (float)width, (float)height, (float)pixel_width / width);
    if (driver)
        draw_driver_display(display.vg, bounds, data);
    else
        draw_vehicle_details(display.vg, bounds, data);
    nvgEndFrame(display.vg);
    SDL_GL_SwapWindow(display.window);
}

#ifdef __EMSCRIPTEN__
enum class WebView { driver, details, split };
struct WebApplication {
    Display display;
    WebView view = WebView::driver;
    SimulatedVehicleDataSource source;
};
static WebApplication web_application;
static void resize_web_canvas() {
    double css_width = 0;
    double css_height = 0;
    if (emscripten_get_element_css_size("#canvas", &css_width, &css_height) !=
        EMSCRIPTEN_RESULT_SUCCESS)
        return;
    const double pixel_ratio =
        std::min(2.0, emscripten_get_device_pixel_ratio());
    const int target_width = std::max(1, (int)(css_width * pixel_ratio));
    const int target_height = std::max(1, (int)(css_height * pixel_ratio));
    int current_width = 0;
    int current_height = 0;
    emscripten_get_canvas_element_size(
        "#canvas", &current_width, &current_height);
    if (target_width != current_width || target_height != current_height)
        emscripten_set_canvas_element_size(
            "#canvas", target_width, target_height);
}
static float web_button_width(float width) {
    return width < 700 ? 76 : 96;
}
static float web_button_start(float width) {
    const float button_width = web_button_width(width);
    return width - 20 - button_width * 3 - 16;
}
static void web_navigation(NVGcontext *vg, float width, WebView view) {
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, width, 56);
    nvgFillColor(vg, nvgRGB(10, 16, 27));
    nvgFill(vg);
    nvgFontSize(vg, 20);
    nvgFontFace(vg, "regular");
    nvgFillColor(vg, nvgRGB(235, 242, 250));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, 20, 28, width < 700 ? "EV" : "EV DASHBOARD", nullptr);
    const char *wide_labels[] = {"DRIVER", "DETAILS", "SPLIT"};
    const char *narrow_labels[] = {"DRV", "INFO", "BOTH"};
    const float button_width = web_button_width(width);
    const float start = web_button_start(width);
    for (int i = 0; i < 3; i++) {
        float x = start + i * (button_width + 8);
        bool selected = (i == 0 && view == WebView::driver) ||
                        (i == 1 && view == WebView::details) ||
                        (i == 2 && view == WebView::split);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, 11, button_width, 34, 8);
        nvgFillColor(vg, selected ? nvgRGB(35, 130, 175) : nvgRGB(25, 38, 55));
        nvgFill(vg);
        nvgFontSize(vg, 12);
        nvgFillColor(vg,
                     selected ? nvgRGB(235, 250, 255) : nvgRGB(135, 157, 180));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg,
                x + button_width / 2,
                28,
                width < 700 ? narrow_labels[i] : wide_labels[i],
                nullptr);
    }
}
static void web_frame(void *arg) {
    auto &app = *static_cast<WebApplication *>(arg);
    if (!app.display.open)
        return;
    resize_web_canvas();
    SDL_Event event;
    int width = 0, height = 0;
    SDL_GetWindowSize(app.display.window, &width, &height);
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT)
            app.display.open = false;
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_1)
                app.view = WebView::driver;
            if (event.key.keysym.sym == SDLK_2)
                app.view = WebView::details;
            if (event.key.keysym.sym == SDLK_3)
                app.view = WebView::split;
            if (event.key.keysym.sym == SDLK_TAB)
                app.view = app.view == WebView::driver    ? WebView::details
                           : app.view == WebView::details ? WebView::split
                                                          : WebView::driver;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.y < 56) {
            const float x = event.button.x;
            const float button_width = web_button_width((float)width);
            const float start = web_button_start((float)width);
            for (int i = 0; i < 3; ++i) {
                const float left = start + i * (button_width + 8);
                if (x >= left && x <= left + button_width)
                    app.view = i == 0   ? WebView::driver
                               : i == 1 ? WebView::details
                                        : WebView::split;
            }
        }
    }
    int pw = 0, ph = 0;
    SDL_GL_GetDrawableSize(app.display.window, &pw, &ph);
    if (width <= 0 || height <= 0 || pw <= 0 || ph <= 0)
        return;
    SDL_GL_MakeCurrent(app.display.window, app.display.context);
    glViewport(0, 0, pw, ph);
    glClearColor(.04f, .06f, .1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(
        app.display.vg, (float)width, (float)height, (float)pw / width);
    web_navigation(app.display.vg, (float)width, app.view);
    Rect content{16, 72, (float)width - 32, (float)height - 88};
    VehicleData data = app.source.sample(emscripten_get_now() / 1000.0);
    if (app.view == WebView::driver)
        draw_driver_display(app.display.vg, content, data);
    else if (app.view == WebView::details)
        draw_vehicle_details(app.display.vg, content, data);
    else {
        float gap = 16;
        if (content.width >= 1100) {
            float dw = (content.width - gap) * 1100 / 1950;
            draw_driver_display(app.display.vg,
                                {content.x, content.y, dw, content.height},
                                data);
            draw_vehicle_details(app.display.vg,
                                 {content.x + dw + gap,
                                  content.y,
                                  content.width - dw - gap,
                                  content.height},
                                 data);
        }
        else {
            float dh = (content.height - gap) * .48f;
            draw_driver_display(app.display.vg,
                                {content.x, content.y, content.width, dh},
                                data);
            draw_vehicle_details(app.display.vg,
                                 {content.x,
                                  content.y + dh + gap,
                                  content.width,
                                  content.height - dh - gap},
                                 data);
        }
    }
    nvgEndFrame(app.display.vg);
    SDL_GL_SwapWindow(app.display.window);
}
#endif

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
#ifdef __EMSCRIPTEN__
    WebApplication &app = web_application;
    resize_web_canvas();
    if (!create_display(app.display,
                        "EV Dashboard",
                        SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED,
                        1200,
                        720,
                        "/assets/fonts/DejaVuSans.ttf")) {
        destroy(app.display);
        SDL_Quit();
        return 1;
    }
    resize_web_canvas();
    int web_width = 0;
    int web_height = 0;
    SDL_GetWindowSize(app.display.window, &web_width, &web_height);
    app.view = web_width >= 1200 ? WebView::split : WebView::driver;
    emscripten_set_main_loop_arg(web_frame, &app, 0, true);
#else
    const char *base_path = SDL_GetBasePath();
    std::string font =
        std::string(base_path ? base_path : "") + "assets/fonts/DejaVuSans.ttf";
    if (base_path)
        SDL_free((void *)base_path);
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
        int driver_width = 0, driver_height = 0, details_width = 0,
            details_height = 0;
        SDL_GetWindowSize(driver.window, &driver_width, &driver_height);
        SDL_GetWindowSize(details.window, &details_width, &details_height);
        if (driver.open)
            render(driver,
                   data,
                   {0, 0, (float)driver_width, (float)driver_height},
                   true);
        if (details.open)
            render(details,
                   data,
                   {0, 0, (float)details_width, (float)details_height},
                   false);
        SDL_Delay(16);
    }
    destroy(details);
    destroy(driver);
    SDL_Quit();
#endif
}
