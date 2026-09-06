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
}
