#include "interactive_vehicle_simulator.h"

#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {
constexpr double vehicle_mass_kg = 1950;
constexpr double battery_capacity_kwh = 75;
constexpr double maximum_drive_power_w = 150000;
constexpr double maximum_reverse_speed_mps = 8.33;
constexpr double air_density = 1.225;
constexpr double drag_area = 0.62;
constexpr double rolling_coefficient = 0.011;
constexpr double gravity = 9.81;
constexpr double drivetrain_efficiency = 0.91;
constexpr double regeneration_efficiency = 0.68;
} // namespace

struct InteractiveVehicleSimulator::PhysicsState {
    btDefaultCollisionConfiguration collision_configuration;
    btCollisionDispatcher dispatcher{&collision_configuration};
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world{
        &dispatcher, &broadphase, &solver, &collision_configuration};
    btStaticPlaneShape ground_shape{btVector3(0, 1, 0), 0};
    btBoxShape car_shape{btVector3(0.95, 0.35, 2.2)};
    std::unique_ptr<btDefaultMotionState> ground_motion;
    std::unique_ptr<btDefaultMotionState> car_motion;
    std::unique_ptr<btRigidBody> ground_body;
    std::unique_ptr<btRigidBody> car_body;
    double heading = 0;

    PhysicsState() {
        world.setGravity(btVector3(0, -gravity, 0));
        btTransform ground_transform;
        ground_transform.setIdentity();
        ground_motion =
            std::make_unique<btDefaultMotionState>(ground_transform);
        btRigidBody::btRigidBodyConstructionInfo ground_info(
            0, ground_motion.get(), &ground_shape);
        ground_info.m_friction = 0;
        ground_body = std::make_unique<btRigidBody>(ground_info);
        world.addRigidBody(ground_body.get());

        btTransform car_transform;
        car_transform.setIdentity();
        car_transform.setOrigin(btVector3(0, 0.36, 0));
        car_motion = std::make_unique<btDefaultMotionState>(car_transform);
        btVector3 inertia;
        car_shape.calculateLocalInertia(vehicle_mass_kg, inertia);
        btRigidBody::btRigidBodyConstructionInfo car_info(
            vehicle_mass_kg, car_motion.get(), &car_shape, inertia);
        car_info.m_friction = 0;
        car_info.m_linearDamping = 0;
        car_info.m_angularDamping = 1;
        car_body = std::make_unique<btRigidBody>(car_info);
        car_body->setAngularFactor(btVector3(0, 0, 0));
        car_body->setActivationState(DISABLE_DEACTIVATION);
        world.addRigidBody(car_body.get());
    }

    ~PhysicsState() {
        world.removeRigidBody(car_body.get());
        world.removeRigidBody(ground_body.get());
    }
};

InteractiveVehicleSimulator::InteractiveVehicleSimulator()
    : physics_(std::make_unique<PhysicsState>()) {}
InteractiveVehicleSimulator::~InteractiveVehicleSimulator() = default;

void InteractiveVehicleSimulator::set_controls(
    const SimulatorControls &controls) {
    controls_ = controls;
}
void InteractiveVehicleSimulator::set_gear(Gear gear) {
    if (std::abs(speed_mps_) < 0.6 || gear == Gear::neutral)
        gear_ = gear;
}
void InteractiveVehicleSimulator::toggle_headlights() {
    visual_.headlights = !visual_.headlights;
    if (!visual_.headlights)
        visual_.high_beam = false;
}
void InteractiveVehicleSimulator::toggle_high_beam() {
    visual_.high_beam = !visual_.high_beam;
    if (visual_.high_beam)
        visual_.headlights = true;
}
void InteractiveVehicleSimulator::toggle_battery_fault() {
    visual_.battery_fault = !visual_.battery_fault;
}
void InteractiveVehicleSimulator::toggle_tire_fault() {
    visual_.tire_fault = !visual_.tire_fault;
}
void InteractiveVehicleSimulator::toggle_drivetrain_fault() {
    visual_.drivetrain_fault = !visual_.drivetrain_fault;
}
void InteractiveVehicleSimulator::toggle_seat_belt() {
    seat_belt_fastened_ = !seat_belt_fastened_;
}
const SimulatorVisualState &InteractiveVehicleSimulator::visual_state() const {
    return visual_;
}

void InteractiveVehicleSimulator::advance(double dt) {
    dt = std::clamp(dt, 0.0, 0.05);
    if (dt <= 0)
        return;

    btRigidBody &body = *physics_->car_body;
    const btVector3 old_velocity = body.getLinearVelocity();
    const double old_speed = std::hypot(old_velocity.x(), old_velocity.z());
    const double throttle = controls_.throttle ? 1.0 : 0.0;
    const double brake = controls_.brake ? 1.0 : 0.0;
    const bool can_drive = gear_ == Gear::drive || gear_ == Gear::reverse;
    const double direction = gear_ == Gear::reverse ? -1.0 : 1.0;
    const double power_limit = visual_.drivetrain_fault || visual_.battery_fault
                                   ? maximum_drive_power_w * 0.25
                                   : maximum_drive_power_w;
    const double traction_magnitude =
        can_drive && throttle > 0
            ? std::min(8500.0, power_limit / std::max(3.0, old_speed))
            : 0;

    const double steer_target = (controls_.steer_right ? 1.0 : 0.0) -
                                (controls_.steer_left ? 1.0 : 0.0);
    visual_.steering +=
        (steer_target - visual_.steering) * std::min(1.0, dt * 5.0);
    physics_->heading += visual_.steering * old_speed * dt * 0.018;
    const btVector3 forward(
        std::sin(physics_->heading), 0, std::cos(physics_->heading));
    const btVector3 right(forward.z(), 0, -forward.x());
    const btVector3 horizontal_velocity(old_velocity.x(), 0, old_velocity.z());
    const double forward_speed = horizontal_velocity.dot(forward);
    const double speed_abs = horizontal_velocity.length();

    body.clearForces();
    body.applyCentralForce(forward * (traction_magnitude * direction));
    if (speed_abs > 0.02) {
        const double resistance =
            0.5 * air_density * drag_area * speed_abs * speed_abs +
            rolling_coefficient * vehicle_mass_kg * gravity;
        body.applyCentralForce(-horizontal_velocity.normalized() * resistance);
    }
    const double brake_magnitude =
        brake * 10000 + (controls_.emergency_brake ? 18000 : 0);
    if (speed_abs > 0.02)
        body.applyCentralForce(-horizontal_velocity.normalized() *
                               brake_magnitude);
    const double lateral_speed = horizontal_velocity.dot(right);
    body.applyCentralForce(right * (-lateral_speed * vehicle_mass_kg * 4.5));

    physics_->world.stepSimulation(dt, 2, 1.0 / 120.0);
    btVector3 velocity = body.getLinearVelocity();
    speed_mps_ = std::hypot(velocity.x(), velocity.z());
    if (gear_ == Gear::park) {
        velocity.setX(0);
        velocity.setZ(0);
        body.setLinearVelocity(velocity);
        speed_mps_ = 0;
    }
    if (gear_ == Gear::reverse && speed_mps_ > maximum_reverse_speed_mps) {
        velocity *= maximum_reverse_speed_mps / speed_mps_;
        body.setLinearVelocity(velocity);
        speed_mps_ = maximum_reverse_speed_mps;
    }
    if (gear_ == Gear::drive && forward_speed < -0.2) {
        velocity.setX(0);
        velocity.setZ(0);
        body.setLinearVelocity(velocity);
        speed_mps_ = 0;
    }

    btTransform transform = body.getWorldTransform();
    transform.setRotation(btQuaternion(btVector3(0, 1, 0), physics_->heading));
    body.setWorldTransform(transform);
    body.getMotionState()->setWorldTransform(transform);

    const double average_speed = (old_speed + speed_mps_) * 0.5;
    const double travelled_m = average_speed * dt;
    visual_.distance_m += travelled_m;
    trip_distance_km_ += travelled_m / 1000;
    const double mechanical_drive_w = traction_magnitude * average_speed;
    const double regenerative_w =
        brake_magnitude > 0 && average_speed > 1
            ? std::min(50000.0, brake_magnitude * average_speed)
            : 0;
    if (mechanical_drive_w > 0) {
        const double electrical_kwh =
            mechanical_drive_w / drivetrain_efficiency * dt / 3600000.0;
        energy_used_kwh_ += electrical_kwh;
        battery_energy_kwh_ -= electrical_kwh;
    }
    if (regenerative_w > 0) {
        const double recovered_kwh =
            regenerative_w * regeneration_efficiency * dt / 3600000.0;
        energy_regenerated_kwh_ += recovered_kwh;
        battery_energy_kwh_ =
            std::min(battery_capacity_kwh, battery_energy_kwh_ + recovered_kwh);
    }
    last_power_kw_ = mechanical_drive_w > 0
                         ? mechanical_drive_w / drivetrain_efficiency / 1000
                         : -regenerative_w * regeneration_efficiency / 1000;
    battery_energy_kwh_ =
        std::clamp(battery_energy_kwh_, 0.0, battery_capacity_kwh);

    visual_.lateral_position_m = transform.getOrigin().x();
    visual_.speed_kph = speed_mps_ * 3.6;
    const double load = mechanical_drive_w / maximum_drive_power_w;
    battery_temperature_c_ +=
        ((23 + load * 18) - battery_temperature_c_) * dt * 0.025;
    motor_temperature_c_ +=
        ((25 + load * 65) - motor_temperature_c_) * dt * 0.04;
    inverter_temperature_c_ +=
        ((25 + load * 48) - inverter_temperature_c_) * dt * 0.05;
}

VehicleData InteractiveVehicleSimulator::sample(double elapsed_seconds) {
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
    data.power_kw = last_power_kw_;
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
