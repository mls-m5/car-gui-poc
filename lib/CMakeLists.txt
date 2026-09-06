include(FetchContent)

FetchContent_Declare(
    nanovg
    GIT_REPOSITORY https://github.com/memononen/nanovg.git
    GIT_TAG ce3bf745eb2d2dbc14a50bf2446783f691ac4353
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nanovg)

add_library(nanovg STATIC
    "${nanovg_SOURCE_DIR}/src/nanovg.c"
)

add_library(NanoVG::NanoVG ALIAS nanovg)

target_include_directories(nanovg
    PUBLIC
        "${nanovg_SOURCE_DIR}/src"
)

set_target_properties(nanovg PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
