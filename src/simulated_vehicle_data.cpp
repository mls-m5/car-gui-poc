#include "vehicle_data.h"
#include <cmath>
namespace {
double clamp(double v, double a, double b) {
    return v < a ? a : v > b ? b : v;
}
} // namespace
VehicleData SimulatedVehicleDataSource::sample(double t) {
    VehicleData d;
    d.simulation_time_seconds = t;
    double speed = 62 + 58 * std::sin(t * .23) - 18 * std::sin(t * .51);
    d.speed_kph = clamp(speed, 0, 130);
    double phase = std::fmod(t, 48.0);
    d.gear = phase < 3   ? Gear::park
             : phase < 6 ? Gear::reverse
             : phase < 9 ? Gear::neutral
                         : Gear::drive;
    if (d.gear != Gear::drive)
        d.speed_kph = 0;
    d.drive_mode = (static_cast<int>(t / 18) % 3 == 0)
                       ? DriveMode::eco
                       : (static_cast<int>(t / 18) % 3 == 1 ? DriveMode::normal
                                                            : DriveMode::sport);
    d.battery_soc_percent = clamp(61 + 27 * std::sin(t * .018), 5, 95);
    d.estimated_range_km = d.battery_soc_percent * 4.65;
    double acceleration = 13 * std::cos(t * .23) - 9 * std::cos(t * .51);
    d.power_kw = clamp(d.speed_kph * 0.42 + acceleration * 3.2, -45, 110);
    d.consumption_kwh_per_100km =
        clamp(15 + d.power_kw * .055 + 3 * std::sin(t * .37), 5, 35);
    // Integral of a smooth representative speed keeps trip counters monotonic.
    d.trip_distance_km = 0.02 * (80 * t + 58 * std::sin(.23 * t) / .23 -
                                 18 * std::sin(.51 * t) / .51);
    d.odometer_km = 18432 + d.trip_distance_km;
    d.outside_temperature_c = 14 + 5 * std::sin(t * .07);
    d.battery_soh_percent = 97 + 1.2 * std::sin(t * .01);
    d.pack_voltage_v = 382 + 12 * std::sin(t * .11);
    d.pack_current_a = d.power_kw * 1000 / d.pack_voltage_v;
    d.battery_temperature_c =
        28 + 8 * std::sin(t * .05) + std::max(0.0, d.power_kw) * .035;
    d.battery_min_temperature_c = d.battery_temperature_c - 2.5;
    d.battery_max_temperature_c = d.battery_temperature_c + 3.1;
    d.minimum_cell_voltage_v = 3.62 + .05 * std::sin(t * .13);
    d.maximum_cell_voltage_v =
        d.minimum_cell_voltage_v + .012 + .012 * (.5 + .5 * std::sin(t * .31));
    d.cell_voltage_delta_mv =
        (d.maximum_cell_voltage_v - d.minimum_cell_voltage_v) * 1000;
    d.twelve_volt_voltage_v = 13.8 + .35 * std::sin(t * .4);
    d.available_discharge_power_kw =
        110 - std::max(0.0, d.battery_temperature_c - 45) * 2;
    d.available_regen_power_kw = 45;
    d.motor_temperature_c =
        35 + std::max(0.0, d.power_kw) * .32 + 8 * std::sin(t * .09);
    d.inverter_temperature_c = 32 + std::max(0.0, d.power_kw) * .22;
    d.tire_pressure_front_left_bar = 2.55 + .06 * std::sin(t * .17);
    d.tire_pressure_front_right_bar = 2.58 + .05 * std::sin(t * .19);
    d.tire_pressure_rear_left_bar = 2.52 + .05 * std::sin(t * .21);
    d.tire_pressure_rear_right_bar = 2.56 + .05 * std::sin(t * .23);
    bool low = std::fmod(t, 60.0) > 38 && std::fmod(t, 60.0) < 48;
    if (low)
        d.tire_pressure_rear_right_bar = 2.05;
    d.average_consumption_kwh_per_100km =
        clamp(17 + 2 * std::sin(t * .08), 5, 35);
    d.trip_energy_used_kwh =
        d.trip_distance_km * d.average_consumption_kwh_per_100km / 100.0;
    d.trip_energy_regenerated_kwh = d.trip_distance_km * .035;
    double blink = std::fmod(t, 8.0);
    d.warnings.left_indicator = blink < 1.5;
    d.warnings.right_indicator = blink > 4 && blink < 5.5;
    d.warnings.high_beam = std::fmod(t, 20) > 12;
    d.warnings.seat_belt = d.speed_kph < 2 && std::fmod(t, 18) < 5;
    d.warnings.parking_brake = d.gear == Gear::park;
    d.warnings.tire_pressure = low;
    d.warnings.battery_warning = std::fmod(t, 75) > 62 && std::fmod(t, 75) < 67;
    d.warnings.general_warning = std::fmod(t, 55) > 27 && std::fmod(t, 55) < 30;
    d.warnings.headlights = true;
    d.charge_state = std::fmod(t, 50) > 42 ? ChargeState::charging
                                           : ChargeState::disconnected;
    return d;
}
const char *gear_name(Gear g) {
    return g == Gear::park      ? "P"
           : g == Gear::reverse ? "R"
           : g == Gear::neutral ? "N"
                                : "D";
}
const char *mode_name(DriveMode m) {
    return m == DriveMode::eco     ? "ECO"
           : m == DriveMode::sport ? "SPORT"
                                   : "NORMAL";
}
