#pragma once

#include "vehicle_data.h"

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
    double world_position_x_m = 0;
    double world_position_z_m = 0;
    double steering = 0;
    double heading_radians = 0;
    double speed_kph = 0;
    bool headlights = false;
    bool high_beam = false;
    bool battery_fault = false;
    bool tire_fault = false;
    bool drivetrain_fault = false;
};

class InteractiveVehicleSimulator : public VehicleDataSource {
public:
    ~InteractiveVehicleSimulator() override = default;
    virtual void set_controls(const SimulatorControls &controls) = 0;
    virtual void set_gear(Gear gear) = 0;
    virtual void toggle_headlights() = 0;
    virtual void toggle_high_beam() = 0;
    virtual void toggle_battery_fault() = 0;
    virtual void toggle_tire_fault() = 0;
    virtual void toggle_drivetrain_fault() = 0;
    virtual void toggle_seat_belt() = 0;
    virtual const SimulatorVisualState &visual_state() const = 0;
};
