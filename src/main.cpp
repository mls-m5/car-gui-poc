#include "bullet_vehicle_simulator.h"
#include "dashboard.h"
#include "dashboard_glow.h"
#include "simulator_3d.h"
#include "simulator_view.h"
#include <GLES2/gl2.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

struct Display {
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    NVGcontext *vg = nullptr;
    std::unique_ptr<Simulator3DRenderer> renderer_3d;
    std::unique_ptr<DashboardGlowRenderer> glow_renderer;
    DriverDashboardAnimation dashboard_animation;
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
    display.glow_renderer = std::make_unique<DashboardGlowRenderer>();
    if (!display.glow_renderer->initialize()) {
        std::cerr << "Could not initialize dashboard glow renderer\n";
        return false;
    }
    display.open = true;
    return true;
}

static bool initialize_3d(Display &display) {
    if (SDL_GL_MakeCurrent(display.window, display.context) != 0)
        return false;
    display.renderer_3d = std::make_unique<Simulator3DRenderer>();
    return display.renderer_3d->initialize();
}

static void destroy(Display &display) {
    if (display.glow_renderer) {
        SDL_GL_MakeCurrent(display.window, display.context);
        display.glow_renderer.reset();
    }
    if (display.renderer_3d) {
        SDL_GL_MakeCurrent(display.window, display.context);
        display.renderer_3d.reset();
    }
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

enum class DisplayContent { driver, details, simulator_3d };

static void render(
    Display &display,
    const VehicleData &data,
    DisplayContent content,
    const SimulatorVisualState *visual = nullptr,
    DriverDashboardStyle dashboard_style = DriverDashboardStyle::modern_rings) {
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (content == DisplayContent::simulator_3d && visual &&
        display.renderer_3d)
        display.renderer_3d->render(0, 0, pixel_width, pixel_height, *visual);
    const Rect bounds{0, 0, (float)width, (float)height};
    nvgBeginFrame(
        display.vg, (float)width, (float)height, (float)pixel_width / width);
    if (content == DisplayContent::driver)
        draw_driver_display(display.vg,
                            bounds,
                            data,
                            dashboard_style,
                            &display.dashboard_animation);
    else if (content == DisplayContent::details)
        draw_vehicle_details(display.vg, bounds, data);
    if (content == DisplayContent::simulator_3d && visual)
        draw_simulator_hud(display.vg, bounds, *visual);
    nvgEndFrame(display.vg);
    if (content == DisplayContent::driver && display.glow_renderer)
        display.glow_renderer->render(pixel_width,
                                      pixel_height,
                                      (float)width,
                                      (float)height,
                                      bounds,
                                      data,
                                      dashboard_style,
                                      display.dashboard_animation);
    SDL_GL_SwapWindow(display.window);
}

static void update_simulator_controls(InteractiveVehicleSimulator &simulator) {
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    SimulatorControls controls;
    controls.throttle = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    controls.brake = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
    controls.steer_left = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    controls.steer_right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
    controls.emergency_brake = keys[SDL_SCANCODE_SPACE];
    simulator.set_controls(controls);
}

static void handle_simulator_toggle(InteractiveVehicleSimulator &simulator,
                                    const SDL_KeyboardEvent &event) {
    if (event.repeat)
        return;
    switch (event.keysym.sym) {
    case SDLK_p:
        simulator.set_gear(Gear::park);
        break;
    case SDLK_r:
        simulator.set_gear(Gear::reverse);
        break;
    case SDLK_n:
        simulator.set_gear(Gear::neutral);
        break;
    case SDLK_g:
        simulator.set_gear(Gear::drive);
        break;
    case SDLK_l:
        simulator.toggle_headlights();
        break;
    case SDLK_h:
        simulator.toggle_high_beam();
        break;
    case SDLK_b:
        simulator.toggle_battery_fault();
        break;
    case SDLK_t:
        simulator.toggle_tire_fault();
        break;
    case SDLK_f:
        simulator.toggle_drivetrain_fault();
        break;
    case SDLK_x:
        simulator.toggle_seat_belt();
        break;
    case SDLK_k:
        simulator.toggle_lane_assist();
        break;
    case SDLK_EQUALS:
    case SDLK_PLUS:
    case SDLK_KP_PLUS:
        simulator.increase_cruise_speed();
        break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
        simulator.decrease_cruise_speed();
        break;
    case SDLK_c:
        simulator.toggle_cruise_control();
        break;
    default:
        break;
    }
}

#ifdef __EMSCRIPTEN__
enum class WebView { simulator_3d, driver, details, combined };
struct WebApplication {
    Display display;
    WebView view = WebView::combined;
    BulletVehicleSimulator simulator_3d;
    InteractiveVehicleSimulator *active_source = &simulator_3d;
    DriverDashboardStyle dashboard_style = DriverDashboardStyle::modern_rings;
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
    return width < 700 ? 54 : 86;
}
static float web_button_start(float width) {
    const float button_width = web_button_width(width);
    return width - (width < 700 ? 10 : 20) - button_width * 4 - 24;
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
    nvgText(vg, 20, 28, width < 700 ? "" : "EV DASHBOARD", nullptr);
    const char *wide_labels[] = {"3D DRIVE", "DRIVER", "DETAILS", "ALL"};
    const char *narrow_labels[] = {"3D", "DRV", "INFO", "ALL"};
    const float button_width = web_button_width(width);
    const float start = web_button_start(width);
    for (int i = 0; i < 4; i++) {
        float x = start + i * (button_width + 8);
        bool selected = (i == 0 && view == WebView::simulator_3d) ||
                        (i == 1 && view == WebView::driver) ||
                        (i == 2 && view == WebView::details) ||
                        (i == 3 && view == WebView::combined);
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
            handle_simulator_toggle(*app.active_source, event.key);
            if (event.key.keysym.sym == SDLK_v && !event.key.repeat)
                app.dashboard_style =
                    app.dashboard_style == DriverDashboardStyle::modern_rings
                        ? DriverDashboardStyle::legacy
                        : DriverDashboardStyle::modern_rings;
            if (event.key.keysym.sym == SDLK_1)
                app.view = WebView::simulator_3d;
            if (event.key.keysym.sym == SDLK_2)
                app.view = WebView::driver;
            if (event.key.keysym.sym == SDLK_3)
                app.view = WebView::details;
            if (event.key.keysym.sym == SDLK_4)
                app.view = WebView::combined;
            if (event.key.keysym.sym == SDLK_TAB)
                app.view = app.view == WebView::simulator_3d ? WebView::driver
                           : app.view == WebView::driver     ? WebView::details
                           : app.view == WebView::details
                               ? WebView::combined
                               : WebView::simulator_3d;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.y < 56) {
            const float x = event.button.x;
            const float button_width = web_button_width((float)width);
            const float start = web_button_start((float)width);
            for (int i = 0; i < 4; ++i) {
                const float left = start + i * (button_width + 8);
                if (x >= left && x <= left + button_width)
                    app.view = i == 0   ? WebView::simulator_3d
                               : i == 1 ? WebView::driver
                               : i == 2 ? WebView::details
                                        : WebView::combined;
            }
        }
    }
    if (app.view == WebView::simulator_3d || app.view == WebView::combined)
        app.active_source = &app.simulator_3d;
    update_simulator_controls(*app.active_source);
    VehicleData data = app.active_source->sample(emscripten_get_now() / 1000.0);
    int pw = 0, ph = 0;
    SDL_GL_GetDrawableSize(app.display.window, &pw, &ph);
    if (width <= 0 || height <= 0 || pw <= 0 || ph <= 0)
        return;
    SDL_GL_MakeCurrent(app.display.window, app.display.context);
    glViewport(0, 0, pw, ph);
    glClearColor(.04f, .06f, .1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    Rect content{16, 72, (float)width - 32, (float)height - 88};
    Rect scene = content;
    if (app.view == WebView::combined)
        scene.height = content.height * .56f;
    if ((app.view == WebView::simulator_3d || app.view == WebView::combined) &&
        app.display.renderer_3d) {
        const float pixel_ratio_x = (float)pw / width;
        const float pixel_ratio_y = (float)ph / height;
        app.display.renderer_3d->render(
            (int)(scene.x * pixel_ratio_x),
            ph - (int)((scene.y + scene.height) * pixel_ratio_y),
            (int)(scene.width * pixel_ratio_x),
            (int)(scene.height * pixel_ratio_y),
            app.simulator_3d.visual_state());
        glViewport(0, 0, pw, ph);
    }
    nvgBeginFrame(
        app.display.vg, (float)width, (float)height, (float)pw / width);
    web_navigation(app.display.vg, (float)width, app.view);
    if (app.view == WebView::simulator_3d)
        draw_simulator_hud(
            app.display.vg, content, app.simulator_3d.visual_state());
    else if (app.view == WebView::driver)
        draw_driver_display(app.display.vg,
                            content,
                            data,
                            app.dashboard_style,
                            &app.display.dashboard_animation);
    else if (app.view == WebView::details)
        draw_vehicle_details(app.display.vg, content, data);
    else {
        constexpr float gap = 12;
        const float panels_y = scene.y + scene.height + gap;
        const float panels_height = content.y + content.height - panels_y;
        const float dashboard_virtual_width =
            app.dashboard_style == DriverDashboardStyle::modern_rings ? 800.f
                                                                      : 1100.f;
        const float driver_width = (content.width - gap) *
                                   dashboard_virtual_width /
                                   (dashboard_virtual_width + 850.f);
        draw_driver_display(app.display.vg,
                            {content.x, panels_y, driver_width, panels_height},
                            data,
                            app.dashboard_style,
                            &app.display.dashboard_animation);
        draw_vehicle_details(app.display.vg,
                             {content.x + driver_width + gap,
                              panels_y,
                              content.width - driver_width - gap,
                              panels_height},
                             data);
        draw_simulator_hud(
            app.display.vg, scene, app.simulator_3d.visual_state());
    }
    nvgEndFrame(app.display.vg);
    if (app.display.glow_renderer && app.view == WebView::driver)
        app.display.glow_renderer->render(pw,
                                          ph,
                                          (float)width,
                                          (float)height,
                                          content,
                                          data,
                                          app.dashboard_style,
                                          app.display.dashboard_animation);
    if (app.display.glow_renderer && app.view == WebView::combined) {
        constexpr float glow_gap = 12;
        const float panels_y = scene.y + scene.height + glow_gap;
        const float panels_height = content.y + content.height - panels_y;
        const float dashboard_virtual_width =
            app.dashboard_style == DriverDashboardStyle::modern_rings ? 800.f
                                                                      : 1100.f;
        const float driver_width = (content.width - glow_gap) *
                                   dashboard_virtual_width /
                                   (dashboard_virtual_width + 850.f);
        app.display.glow_renderer->render(
            pw,
            ph,
            (float)width,
            (float)height,
            {content.x, panels_y, driver_width, panels_height},
            data,
            app.dashboard_style,
            app.display.dashboard_animation);
    }
    SDL_GL_SwapWindow(app.display.window);
}
#endif

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        error("SDL_Init");
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
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
                        "/assets/fonts/DejaVuSans.ttf") ||
        !initialize_3d(app.display)) {
        destroy(app.display);
        SDL_Quit();
        return 1;
    }
    resize_web_canvas();
    app.view = WebView::combined;
    emscripten_set_main_loop_arg(web_frame, &app, 0, true);
#else
    const char *base_path = SDL_GetBasePath();
    std::string font =
        std::string(base_path ? base_path : "") + "assets/fonts/DejaVuSans.ttf";
    if (base_path)
        SDL_free((void *)base_path);
    bool interactive = false;
    DriverDashboardStyle dashboard_style = DriverDashboardStyle::modern_rings;
    for (int i = 1; i < argc; ++i) {
        interactive =
            interactive || std::string(argv[i]) == "--backend=simulator";
        if (std::string(argv[i]) == "--dashboard=legacy")
            dashboard_style = DriverDashboardStyle::legacy;
        if (std::string(argv[i]) == "--dashboard=modern")
            dashboard_style = DriverDashboardStyle::modern_rings;
    }
    Display driver, details, simulator_display;
    bool displays_created =
        create_display(driver,
                       "EV Driver Display",
                       SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED,
                       1100,
                       600,
                       font.c_str()) &&
        create_display(details,
                       "EV Vehicle Details",
                       SDL_WINDOWPOS_CENTERED + 80,
                       SDL_WINDOWPOS_CENTERED + 80,
                       850,
                       650,
                       font.c_str()) &&
        (!interactive || create_display(simulator_display,
                                        "EV Driving Simulator",
                                        SDL_WINDOWPOS_CENTERED + 160,
                                        SDL_WINDOWPOS_CENTERED + 160,
                                        1000,
                                        650,
                                        font.c_str()));
    if (displays_created && interactive)
        displays_created = initialize_3d(simulator_display);
    if (!displays_created) {
        destroy(simulator_display);
        destroy(details);
        destroy(driver);
        SDL_Quit();
        return 1;
    }
    SimulatedVehicleDataSource dummy_source;
    BulletVehicleSimulator simulator_3d;
    InteractiveVehicleSimulator *interactive_source = &simulator_3d;
    VehicleDataSource *source =
        interactive ? static_cast<VehicleDataSource *>(interactive_source)
                    : static_cast<VehicleDataSource *>(&dummy_source);
    auto start = std::chrono::steady_clock::now();
    bool running = true;
    while (running && (driver.open || details.open || simulator_display.open)) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = false;
                if (event.key.keysym.sym == SDLK_v && !event.key.repeat)
                    dashboard_style =
                        dashboard_style == DriverDashboardStyle::modern_rings
                            ? DriverDashboardStyle::legacy
                            : DriverDashboardStyle::modern_rings;
                if (interactive)
                    handle_simulator_toggle(*interactive_source, event.key);
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (driver.window &&
                    event.window.windowID == SDL_GetWindowID(driver.window))
                    driver.open = false;
                if (details.window &&
                    event.window.windowID == SDL_GetWindowID(details.window))
                    details.open = false;
                if (simulator_display.window &&
                    event.window.windowID ==
                        SDL_GetWindowID(simulator_display.window))
                    simulator_display.open = false;
            }
        }
        if (interactive)
            update_simulator_controls(*interactive_source);
        double time = std::chrono::duration<double>(
                          std::chrono::steady_clock::now() - start)
                          .count();
        VehicleData data = source->sample(time);
        if (driver.open)
            render(
                driver, data, DisplayContent::driver, nullptr, dashboard_style);
        if (details.open)
            render(details, data, DisplayContent::details);
        if (simulator_display.open)
            render(simulator_display,
                   data,
                   DisplayContent::simulator_3d,
                   &interactive_source->visual_state());
        SDL_Delay(16);
    }
    destroy(simulator_display);
    destroy(details);
    destroy(driver);
    SDL_Quit();
#endif
}
