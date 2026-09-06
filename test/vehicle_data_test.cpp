#include "bullet_vehicle_simulator.h"
#include "two_d_vehicle_simulator.h"
#include "vehicle_data.h"
#include <cassert>
#include <cmath>
int main() {
    SimulatedVehicleDataSource source;
    double previous_distance = 0;
    for (int i = 0; i != 120; ++i) {
        VehicleData d = source.sample(i * 0.5);
        assert(d.speed_kph >= 0 && d.speed_kph <= 130);
        assert(d.battery_soc_percent >= 5 && d.battery_soc_percent <= 95);
        assert(d.minimum_cell_voltage_v <= d.maximum_cell_voltage_v);
        assert(std::abs(d.cell_voltage_delta_mv -
                        (d.maximum_cell_voltage_v - d.minimum_cell_voltage_v) *
                            1000) < 0.01);
        assert(d.battery_min_temperature_c <= d.battery_temperature_c);
        assert(d.battery_temperature_c <= d.battery_max_temperature_c);
        assert(d.tire_pressure_front_left_bar > 0 &&
               d.tire_pressure_rear_right_bar > 0);
        assert(d.trip_distance_km >= previous_distance);
        previous_distance = d.trip_distance_km;
    }

    BulletVehicleSimulator simulator;
    SimulatorControls controls;
    controls.throttle = true;
    simulator.set_controls(controls);
    simulator.sample(0);
    VehicleData driven;
    for (int i = 1; i <= 400; ++i)
        driven = simulator.sample(i * 0.025);
    assert(driven.speed_kph > 10);
    assert(driven.trip_distance_km > 0);
    assert(driven.trip_energy_used_kwh > 0);
    assert(driven.battery_soc_percent < 64);

    controls.steer_right = true;
    simulator.set_controls(controls);
    for (int i = 401; i <= 480; ++i)
        simulator.sample(i * 0.025);
    assert(std::abs(simulator.visual_state().heading_radians) > 0.01);
    assert(std::abs(simulator.visual_state().lateral_position_m) > 0.01);

    controls.throttle = false;
    controls.brake = true;
    controls.steer_right = false;
    simulator.set_controls(controls);
    VehicleData braked;
    for (int i = 481; i <= 680; ++i)
        braked = simulator.sample(i * 0.025);
    assert(braked.speed_kph < driven.speed_kph);
    assert(braked.trip_energy_regenerated_kwh > 0);

    simulator.toggle_tire_fault();
    simulator.toggle_battery_fault();
    VehicleData faulted = simulator.sample(17.1);
    assert(faulted.warnings.tire_pressure);
    assert(faulted.warnings.battery_warning);
    assert(faulted.tire_pressure_rear_right_bar < 2.2);

    TwoDVehicleSimulator two_d_simulator;
    controls.brake = false;
    controls.throttle = true;
    two_d_simulator.set_controls(controls);
    two_d_simulator.sample(0);
    VehicleData two_d_driven;
    for (int i = 1; i <= 400; ++i)
        two_d_driven = two_d_simulator.sample(i * 0.025);
    assert(two_d_driven.speed_kph > 10);
    assert(two_d_driven.trip_energy_used_kwh > 0);
}
