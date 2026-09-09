#pragma once
#include <string>

enum class Gear { park, reverse, neutral, drive };
enum class DriveMode { eco, normal, sport };
enum class ChargeState { disconnected, charging, complete };
struct WarningStates {
    bool left_indicator = false, right_indicator = false, high_beam = false,
         headlights = true, seat_belt = false, parking_brake = false,
         tire_pressure = false, battery_warning = false,
         general_warning = false, lane_assist = false, cruise_control = false;
};
struct VehicleData {
    double simulation_time_seconds = 0, speed_kph = 0, battery_soc_percent = 0,
           estimated_range_km = 0, power_kw = 0, consumption_kwh_per_100km = 0,
           trip_distance_km = 0, odometer_km = 0, outside_temperature_c = 0,
           cruise_control_target_kph = -1;
    Gear gear = Gear::drive;
    DriveMode drive_mode = DriveMode::normal;
    WarningStates warnings;
    ChargeState charge_state = ChargeState::disconnected;
    double battery_soh_percent = 0, pack_voltage_v = 0, pack_current_a = 0,
           battery_temperature_c = 0, battery_min_temperature_c = 0,
           battery_max_temperature_c = 0, minimum_cell_voltage_v = 0,
           maximum_cell_voltage_v = 0, cell_voltage_delta_mv = 0,
           twelve_volt_voltage_v = 0, available_discharge_power_kw = 0,
           available_regen_power_kw = 0, motor_temperature_c = 0,
           inverter_temperature_c = 0, tire_pressure_front_left_bar = 0,
           tire_pressure_front_right_bar = 0, tire_pressure_rear_left_bar = 0,
           tire_pressure_rear_right_bar = 0, trip_energy_used_kwh = 0,
           trip_energy_regenerated_kwh = 0,
           average_consumption_kwh_per_100km = 0;
};
// Dashboard-facing decoded telemetry boundary. A future implementation can own
// an Ethernet transport and packet decoder while leaving all UI code unchanged.
class VehicleDataSource {
public:
    virtual ~VehicleDataSource() = default;
    virtual VehicleData sample(double seconds) = 0;
};
class SimulatedVehicleDataSource final : public VehicleDataSource {
public:
    VehicleData sample(double seconds) override;
};
const char *gear_name(Gear);
const char *mode_name(DriveMode);
