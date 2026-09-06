#pragma once

#include "interactive_vehicle_simulator.h"

class TwoDVehicleSimulator final : public InteractiveVehicleSimulator {
public:
    VehicleData sample(double elapsed_seconds) override;
    void set_controls(const SimulatorControls &controls) override;
    void set_gear(Gear gear) override;
    void toggle_headlights() override;
    void toggle_high_beam() override;
    void toggle_battery_fault() override;
    void toggle_tire_fault() override;
    void toggle_drivetrain_fault() override;
    void toggle_seat_belt() override;
    const SimulatorVisualState &visual_state() const override;

private:
    void advance(double dt);

    SimulatorControls controls_;
    SimulatorVisualState visual_;
    Gear gear_ = Gear::drive;
    double previous_time_ = -1;
    double speed_mps_ = 0;
    double battery_energy_kwh_ = 48;
    double trip_distance_km_ = 0;
    double energy_used_kwh_ = 0;
    double energy_regenerated_kwh_ = 0;
    double battery_temperature_c_ = 24;
    double motor_temperature_c_ = 24;
    double inverter_temperature_c_ = 24;
    bool seat_belt_fastened_ = true;
};
