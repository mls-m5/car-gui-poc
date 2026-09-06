add_executable(vehicle-data-test
    vehicle_data_test.cpp
    ${CMAKE_SOURCE_DIR}/src/simulated_vehicle_data.cpp
)
target_include_directories(vehicle-data-test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(vehicle-data-test PRIVATE cxx_std_17)
add_test(NAME vehicle-data-test COMMAND vehicle-data-test)
