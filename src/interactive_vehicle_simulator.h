#pragma once

#include "vehicle_data.h"

#include <memory>

struct SimulatorControls {
    bool throttle = false;
    bool brake = false;
    bool steer_left = false;
    bool steer_right = false;
    bool emergency_brake = false;
};

struct SimulatorVisualState {
    double distance_m = 0;
    double lateral_position_m = 0;
    double steering = 0;
    double speed_kph = 0;
    bool headlights = false;
    bool high_beam = false;
    bool battery_fault = false;
    bool tire_fault = false;
    bool drivetrain_fault = false;
};

class InteractiveVehicleSimulator final : public VehicleDataSource {
public:
    InteractiveVehicleSimulator();
    ~InteractiveVehicleSimulator() override;
    InteractiveVehicleSimulator(const InteractiveVehicleSimulator &) = delete;
    InteractiveVehicleSimulator &operator=(
        const InteractiveVehicleSimulator &) = delete;

    VehicleData sample(double elapsed_seconds) override;
    void set_controls(const SimulatorControls &controls);
    void set_gear(Gear gear);
    void toggle_headlights();
    void toggle_high_beam();
    void toggle_battery_fault();
    void toggle_tire_fault();
    void toggle_drivetrain_fault();
    void toggle_seat_belt();
    const SimulatorVisualState &visual_state() const;

private:
    struct PhysicsState;
    void advance(double dt);

    std::unique_ptr<PhysicsState> physics_;
    SimulatorControls controls_;
    SimulatorVisualState visual_;
    Gear gear_ = Gear::drive;
    double previous_time_ = -1;
    double speed_mps_ = 0;
    double last_power_kw_ = 0;
    double battery_energy_kwh_ = 48;
    double trip_distance_km_ = 0;
    double energy_used_kwh_ = 0;
    double energy_regenerated_kwh_ = 0;
    double battery_temperature_c_ = 24;
    double motor_temperature_c_ = 24;
    double inverter_temperature_c_ = 24;
    bool seat_belt_fastened_ = true;
};
