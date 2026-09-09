#include "bullet_vehicle_simulator.h"
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

    BulletVehicleSimulator gentle_braking_simulator;
    SimulatorControls gentle_controls;
    gentle_controls.throttle = true;
    gentle_braking_simulator.set_controls(gentle_controls);
    gentle_braking_simulator.sample(0);
    VehicleData speed_before_braking;
    for (int i = 1; i <= 240; ++i)
        speed_before_braking = gentle_braking_simulator.sample(i * 0.025);
    gentle_controls.throttle = false;
    gentle_controls.brake = true;
    gentle_braking_simulator.set_controls(gentle_controls);
    VehicleData first_braking_frame = gentle_braking_simulator.sample(6.025);
    assert(first_braking_frame.speed_kph >
           speed_before_braking.speed_kph * .85);
    VehicleData after_gentle_braking;
    for (int i = 242; i <= 362; ++i)
        after_gentle_braking = gentle_braking_simulator.sample(i * 0.025);
    assert(after_gentle_braking.speed_kph < speed_before_braking.speed_kph);
    assert(after_gentle_braking.trip_energy_regenerated_kwh > 0);

    BulletVehicleSimulator assisted_simulator;
    SimulatorControls assisted_controls;
    assisted_controls.throttle = true;
    assisted_simulator.set_controls(assisted_controls);
    assisted_simulator.sample(0);
    VehicleData assistance_data;
    for (int i = 1; i <= 240; ++i)
        assistance_data = assisted_simulator.sample(i * .025);
    assisted_controls.throttle = false;
    assisted_simulator.set_controls(assisted_controls);
    assisted_simulator.increase_cruise_speed();
    assistance_data = assisted_simulator.sample(6.025);
    assert(assistance_data.warnings.cruise_control);
    assert(assistance_data.cruise_control_target_kph >= 0);
    const double saved_cruise_target =
        assistance_data.cruise_control_target_kph;
    assisted_simulator.increase_cruise_speed();
    assistance_data = assisted_simulator.sample(6.05);
    assert(std::abs(assistance_data.cruise_control_target_kph -
                    (saved_cruise_target + 1)) < .01);
    assisted_simulator.toggle_cruise_control();
    assistance_data = assisted_simulator.sample(6.075);
    assert(!assistance_data.warnings.cruise_control);
    assert(assistance_data.cruise_control_target_kph > 0);
    assisted_simulator.toggle_cruise_control();
    assisted_simulator.toggle_lane_assist();
    assistance_data = assisted_simulator.sample(6.1);
    assert(assistance_data.warnings.cruise_control);
    assert(assistance_data.warnings.lane_assist);
    for (int i = 245; i <= 324; ++i)
        assistance_data = assisted_simulator.sample(i * .025);
    assert(std::abs(assistance_data.speed_kph -
                    assistance_data.cruise_control_target_kph) < 8);
    assisted_controls.brake = true;
    assisted_simulator.set_controls(assisted_controls);
    assistance_data = assisted_simulator.sample(8.125);
    assert(!assistance_data.warnings.cruise_control);
    assert(assistance_data.cruise_control_target_kph > 0);

    BulletVehicleSimulator lane_simulator;
    SimulatorControls lane_controls;
    lane_controls.throttle = true;
    lane_simulator.set_controls(lane_controls);
    lane_simulator.sample(0);
    for (int i = 1; i <= 160; ++i)
        lane_simulator.sample(i * .025);
    lane_controls.steer_right = true;
    lane_simulator.set_controls(lane_controls);
    for (int i = 161; i <= 210; ++i)
        lane_simulator.sample(i * .025);
    const double distance_from_right_lane =
        std::abs(lane_simulator.visual_state().world_position_x_m + 2.5);
    assert(distance_from_right_lane > .1);
    lane_controls.steer_right = false;
    lane_simulator.set_controls(lane_controls);
    lane_simulator.toggle_lane_assist();
    for (int i = 211; i <= 450; ++i)
        lane_simulator.sample(i * .025);
    assert(std::abs(lane_simulator.visual_state().world_position_x_m + 2.5) <
           distance_from_right_lane);

    simulator.toggle_tire_fault();
    simulator.toggle_battery_fault();
    VehicleData faulted = simulator.sample(17.1);
    assert(faulted.warnings.tire_pressure);
    assert(faulted.warnings.battery_warning);
    assert(faulted.tire_pressure_rear_right_bar < 2.2);
}
