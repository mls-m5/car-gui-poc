cmake_minimum_required(VERSION 3.27)
project(car-gui LANGUAGES C CXX)

enable_testing()
add_subdirectory(lib)

find_package(SDL2 REQUIRED CONFIG)

if(EMSCRIPTEN)
    # Emscripten supplies SDL2 and GLES2 through its toolchain.
    set(CAR_GUI_PLATFORM_LIBRARIES SDL2::SDL2)
else()
    find_package(OpenGL REQUIRED COMPONENTS GLES2)
    set(CAR_GUI_PLATFORM_LIBRARIES SDL2::SDL2 OpenGL::GLES2)
endif()

add_executable(car-gui
    src/main.cpp
    src/nanovg_gles2.cpp
    src/dashboard.cpp
    src/simulated_vehicle_data.cpp
)

target_compile_features(car-gui PRIVATE cxx_std_17)

add_custom_command(TARGET car-gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:car-gui>/assets"
)

target_link_libraries(car-gui
    PRIVATE
        NanoVG::NanoVG
        ${CAR_GUI_PLATFORM_LIBRARIES}
)

if(EMSCRIPTEN)
    target_compile_definitions(car-gui PRIVATE NANOVG_GLES2)
    target_link_options(car-gui PRIVATE
        "-sUSE_SDL=2"
        "-sMIN_WEBGL_VERSION=1"
        "-sMAX_WEBGL_VERSION=1"
    )
endif()

add_subdirectory(test)
