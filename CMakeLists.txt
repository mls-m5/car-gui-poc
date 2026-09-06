cmake_minimum_required(VERSION 3.27)
project(car-gui LANGUAGES C CXX)

enable_testing()
add_subdirectory(lib)

if(EMSCRIPTEN)
    # Emscripten supplies SDL2 and GLES2 through its ports.
    set(CAR_GUI_PLATFORM_LIBRARIES)
else()
    find_package(SDL2 REQUIRED CONFIG)
    find_package(OpenGL REQUIRED COMPONENTS GLES2)
    set(CAR_GUI_PLATFORM_LIBRARIES SDL2::SDL2 OpenGL::GLES2)
endif()

add_executable(car-gui
    src/main.cpp
    src/nanovg_gles2.cpp
    src/dashboard.cpp
    src/simulated_vehicle_data.cpp
    src/interactive_vehicle_simulator.cpp
    src/simulator_view.cpp
)

target_compile_features(car-gui PRIVATE cxx_std_17)

if(NOT EMSCRIPTEN)
    add_custom_command(TARGET car-gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:car-gui>/assets"
    )
endif()

target_link_libraries(car-gui
    PRIVATE
        NanoVG::NanoVG
        ${CAR_GUI_PLATFORM_LIBRARIES}
)

if(EMSCRIPTEN)
    target_compile_options(car-gui PRIVATE "SHELL:-s USE_SDL=2")
    target_link_options(car-gui PRIVATE
        "SHELL:-s USE_SDL=2"
        "SHELL:-s MIN_WEBGL_VERSION=1"
        "SHELL:-s MAX_WEBGL_VERSION=1"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets@/assets"
    )
    set_target_properties(car-gui PROPERTIES SUFFIX ".html")
endif()

if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(test)
endif()
