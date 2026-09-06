
cmake_minimum_required(VERSION 3.23)
project(car-gui)

enable_testing()
add_subdirectory(lib)

add_executable(
    car-gui
    src/main.cpp
    )

target_compile_features(
    car-gui
    PRIVATE
    cxx_std_17
    )

find_package(Threads)
target_link_libraries(
    car-gui
    PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    )

add_subdirectory(test)

