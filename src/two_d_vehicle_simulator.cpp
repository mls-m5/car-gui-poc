#include "two_d_vehicle_simulator.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double vehicle_mass_kg = 1950;
constexpr double battery_capacity_kwh = 75;
constexpr double maximum_drive_power_w = 150000;
constexpr double maximum_reverse_speed_mps = 8.33;
constexpr double air_density = 1.225;
constexpr double drag_area = 0.62;
constexpr double rolling_coefficient = 0.011;
constexpr double gravity = 9.81;
} // namespace

void TwoDVehicleSimulator::set_controls(const SimulatorControls &controls) {
    controls_ = controls;
}

void TwoDVehicleSimulator::set_gear(Gear gear) {
    if (std::abs(speed_mps_) < 0.6 || gear == Gear::neutral)
        gear_ = gear;
}

void TwoDVehicleSimulator::toggle_headlights() {
    visual_.headlights = !visual_.headlights;
    if (!visual_.headlights)
        visual_.high_beam = false;
}

void TwoDVehicleSimulator::toggle_high_beam() {
    visual_.high_beam = !visual_.high_beam;
    if (visual_.high_beam)
        visual_.headlights = true;
}

void TwoDVehicleSimulator::toggle_battery_fault() {
    visual_.battery_fault = !visual_.battery_fault;
}
void TwoDVehicleSimulator::toggle_tire_fault() {
    visual_.tire_fault = !visual_.tire_fault;
}
void TwoDVehicleSimulator::toggle_drivetrain_fault() {
    visual_.drivetrain_fault = !visual_.drivetrain_fault;
}
void TwoDVehicleSimulator::toggle_seat_belt() {
    seat_belt_fastened_ = !seat_belt_fastened_;
}
const SimulatorVisualState &TwoDVehicleSimulator::visual_state() const {
    return visual_;
}

void TwoDVehicleSimulator::advance(double dt) {
    dt = std::clamp(dt, 0.0, 0.05);
    const double throttle = controls_.throttle ? 1.0 : 0.0;
    const double brake = controls_.brake ? 1.0 : 0.0;
    const double direction = gear_ == Gear::reverse ? -1.0 : 1.0;
    const bool can_drive = gear_ == Gear::drive || gear_ == Gear::reverse;
    const double speed_abs = std::abs(speed_mps_);
    const double power_limit = visual_.drivetrain_fault || visual_.battery_fault
                                   ? maximum_drive_power_w * 0.25
                                   : maximum_drive_power_w;
    double traction_force = 0;
    if (can_drive && throttle > 0)
        traction_force =
            direction *
            std::min(8500.0, power_limit / std::max(3.0, speed_abs));

    const double resistance =
        speed_abs > 0.02
            ? std::copysign(0.5 * air_density * drag_area * speed_abs *
                                    speed_abs +
                                rolling_coefficient * vehicle_mass_kg * gravity,
                            speed_mps_)
            : 0;
    const double brake_force =
        (brake * 10000 + (controls_.emergency_brake ? 18000 : 0));
    const double signed_brake =
        speed_abs > 0.02 ? std::copysign(brake_force, speed_mps_) : 0;
    const double acceleration =
        (traction_force - resistance - signed_brake) / vehicle_mass_kg;
    const double old_speed = speed_mps_;
    speed_mps_ += acceleration * dt;
    if ((old_speed > 0 && speed_mps_ < 0) || (old_speed < 0 && speed_mps_ > 0))
        speed_mps_ = 0;
    if (gear_ == Gear::park)
        speed_mps_ = 0;
    if (gear_ == Gear::reverse)
        speed_mps_ = std::clamp(speed_mps_, -maximum_reverse_speed_mps, 0.0);
    if (gear_ == Gear::drive)
        speed_mps_ = std::clamp(speed_mps_, 0.0, 55.0);

    const double travelled_m = std::abs(speed_mps_) * dt;
    visual_.distance_m += travelled_m;
    trip_distance_km_ += travelled_m / 1000;
    const double drive_power_w = traction_force * speed_mps_;
    const double regen_power_w =
        brake > 0 && speed_abs > 1
            ? std::min(50000.0, brake_force * speed_abs * 0.55)
            : 0;
    if (drive_power_w > 0) {
        const double used = drive_power_w * dt / 3600000000.0;
        energy_used_kwh_ += used;
        battery_energy_kwh_ -= used;
    }
    if (regen_power_w > 0) {
        const double recovered = regen_power_w * dt / 3600000000.0;
        energy_regenerated_kwh_ += recovered;
        battery_energy_kwh_ =
            std::min(battery_capacity_kwh, battery_energy_kwh_ + recovered);
    }

    const double steer_target = (controls_.steer_right ? 1.0 : 0.0) -
                                (controls_.steer_left ? 1.0 : 0.0);
    visual_.steering +=
        (steer_target - visual_.steering) * std::min(1.0, dt * 5.0);
    visual_.lateral_position_m += visual_.steering * speed_abs * dt * 0.16;
    visual_.lateral_position_m *= std::max(0.0, 1.0 - dt * 0.08);
    visual_.lateral_position_m =
        std::clamp(visual_.lateral_position_m, -5.5, 5.5);
    visual_.world_position_x_m = visual_.lateral_position_m;
    visual_.world_position_z_m = visual_.distance_m;
    visual_.speed_kph = speed_abs * 3.6;

    const double load = std::abs(drive_power_w) / maximum_drive_power_w;
    battery_temperature_c_ +=
        ((23 + load * 18) - battery_temperature_c_) * dt * 0.025;
    motor_temperature_c_ +=
        ((25 + load * 65) - motor_temperature_c_) * dt * 0.04;
    inverter_temperature_c_ +=
        ((25 + load * 48) - inverter_temperature_c_) * dt * 0.05;
}

VehicleData TwoDVehicleSimulator::sample(double elapsed_seconds) {
    if (previous_time_ < 0)
        previous_time_ = elapsed_seconds;
    advance(elapsed_seconds - previous_time_);
    previous_time_ = elapsed_seconds;

    VehicleData data;
    data.simulation_time_seconds = elapsed_seconds;
    data.speed_kph = visual_.speed_kph;
    data.gear = gear_;
    data.drive_mode = DriveMode::normal;
    data.battery_soc_percent = 100 * battery_energy_kwh_ / battery_capacity_kwh;
    data.estimated_range_km = data.battery_soc_percent * 4.7;
    const double throttle_power =
        controls_.throttle ? std::min(150.0, 12.0 + data.speed_kph * 1.15) : 0;
    const double regen_power =
        (controls_.brake || controls_.emergency_brake) && data.speed_kph > 4
            ? -std::min(50.0, data.speed_kph * 0.75)
            : 0;
    data.power_kw = regen_power != 0 ? regen_power : throttle_power;
    if (visual_.drivetrain_fault || visual_.battery_fault)
        data.power_kw *= 0.25;
    data.consumption_kwh_per_100km = data.speed_kph > 2 && data.power_kw > 0
                                         ? data.power_kw / data.speed_kph * 100
                                         : 0;
    data.trip_distance_km = trip_distance_km_;
    data.odometer_km = 18432 + trip_distance_km_;
    data.outside_temperature_c = 14;
    data.battery_soh_percent = 97.4;
    data.pack_voltage_v = 350 + data.battery_soc_percent * 0.55;
    data.pack_current_a = data.pack_voltage_v > 0
                              ? data.power_kw * 1000 / data.pack_voltage_v
                              : 0;
    data.battery_temperature_c = battery_temperature_c_;
    data.battery_min_temperature_c = battery_temperature_c_ - 1.5;
    data.battery_max_temperature_c = battery_temperature_c_ + 2.0;
    data.minimum_cell_voltage_v = 3.45 + data.battery_soc_percent * 0.006;
    data.maximum_cell_voltage_v =
        data.minimum_cell_voltage_v + (visual_.battery_fault ? 0.055 : 0.012);
    data.cell_voltage_delta_mv =
        (data.maximum_cell_voltage_v - data.minimum_cell_voltage_v) * 1000;
    data.twelve_volt_voltage_v = 13.9;
    data.available_discharge_power_kw = visual_.battery_fault ? 35 : 150;
    data.available_regen_power_kw = 50;
    data.motor_temperature_c = motor_temperature_c_;
    data.inverter_temperature_c = inverter_temperature_c_;
    data.tire_pressure_front_left_bar = 2.55;
    data.tire_pressure_front_right_bar = 2.57;
    data.tire_pressure_rear_left_bar = 2.58;
    data.tire_pressure_rear_right_bar = visual_.tire_fault ? 1.85 : 2.56;
    data.trip_energy_used_kwh = energy_used_kwh_;
    data.trip_energy_regenerated_kwh = energy_regenerated_kwh_;
    data.average_consumption_kwh_per_100km =
        trip_distance_km_ > 0.01 ? energy_used_kwh_ / trip_distance_km_ * 100
                                 : 0;
    data.warnings.headlights = visual_.headlights;
    data.warnings.high_beam = visual_.high_beam;
    data.warnings.left_indicator = controls_.steer_left;
    data.warnings.right_indicator = controls_.steer_right;
    data.warnings.seat_belt = !seat_belt_fastened_;
    data.warnings.parking_brake = gear_ == Gear::park;
    data.warnings.tire_pressure = visual_.tire_fault;
    data.warnings.battery_warning = visual_.battery_fault;
    data.warnings.general_warning =
        visual_.drivetrain_fault || std::abs(visual_.lateral_position_m) > 2.8;
    return data;
}
