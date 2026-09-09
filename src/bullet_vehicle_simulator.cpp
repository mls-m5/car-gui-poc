#include "bullet_vehicle_simulator.h"

#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

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
constexpr double normal_wheel_brake_impulse = 160;
constexpr double emergency_wheel_brake_impulse = 1000;
constexpr double maximum_regenerative_force_n = 7200;
constexpr double maximum_regenerative_power_w = 35000;
} // namespace

struct BulletVehicleSimulator::PhysicsState {
    btDefaultCollisionConfiguration collision_configuration;
    btCollisionDispatcher dispatcher{&collision_configuration};
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world{
        &dispatcher, &broadphase, &solver, &collision_configuration};
    btStaticPlaneShape ground_shape{btVector3(0, 1, 0), 0};
    btBoxShape car_shape{btVector3(0.95, 0.35, 2.2)};
    btDefaultVehicleRaycaster raycaster{&world};
    btRaycastVehicle::btVehicleTuning tuning;
    std::unique_ptr<btDefaultMotionState> ground_motion;
    std::unique_ptr<btDefaultMotionState> car_motion;
    std::unique_ptr<btRigidBody> ground_body;
    std::unique_ptr<btRigidBody> car_body;
    std::unique_ptr<btRaycastVehicle> vehicle;
    std::vector<std::unique_ptr<btCollisionShape>> tree_shapes;
    std::vector<std::unique_ptr<btDefaultMotionState>> tree_motions;
    std::vector<std::unique_ptr<btRigidBody>> tree_bodies;
    int tree_sector = std::numeric_limits<int>::min();

    void update_tree_collisions(double car_z) {
        constexpr double spacing = 24.0;
        const int sector = (int)std::floor(car_z / spacing);
        if (sector == tree_sector)
            return;
        for (auto &body : tree_bodies)
            world.removeRigidBody(body.get());
        tree_bodies.clear();
        tree_motions.clear();
        tree_shapes.clear();
        for (int i = sector - 10; i < sector + 21; ++i) {
            const int pattern = ((i % 7) + 7) % 7;
            const double variation = pattern * .72;
            const double z = i * spacing;
            const double positions[2] = {-8.5 - variation, 8.5 + variation};
            const double offsets[2] = {0, 9};
            for (int side = 0; side < 2; ++side) {
                auto shape =
                    std::make_unique<btCylinderShape>(btVector3(.16, .9, .16));
                btTransform transform;
                transform.setIdentity();
                transform.setOrigin(
                    btVector3(positions[side], .9, z + offsets[side]));
                auto motion = std::make_unique<btDefaultMotionState>(transform);
                btRigidBody::btRigidBodyConstructionInfo info(
                    0, motion.get(), shape.get());
                info.m_friction = .8;
                auto body = std::make_unique<btRigidBody>(info);
                world.addRigidBody(body.get());
                tree_shapes.push_back(std::move(shape));
                tree_motions.push_back(std::move(motion));
                tree_bodies.push_back(std::move(body));
            }
        }
        tree_sector = sector;
    }

    PhysicsState() {
        world.setGravity(btVector3(0, -gravity, 0));
        btTransform ground_transform;
        ground_transform.setIdentity();
        ground_motion =
            std::make_unique<btDefaultMotionState>(ground_transform);
        btRigidBody::btRigidBodyConstructionInfo ground_info(
            0, ground_motion.get(), &ground_shape);
        ground_info.m_friction = 1;
        ground_body = std::make_unique<btRigidBody>(ground_info);
        world.addRigidBody(ground_body.get());

        btTransform car_transform;
        car_transform.setIdentity();
        car_transform.setOrigin(btVector3(0, 1.0, 0));
        car_motion = std::make_unique<btDefaultMotionState>(car_transform);
        btVector3 inertia;
        car_shape.calculateLocalInertia(vehicle_mass_kg, inertia);
        btRigidBody::btRigidBodyConstructionInfo car_info(
            vehicle_mass_kg, car_motion.get(), &car_shape, inertia);
        car_info.m_friction = 0.2;
        car_info.m_angularDamping = 0.7;
        car_body = std::make_unique<btRigidBody>(car_info);
        car_body->setAngularFactor(btVector3(0, 1, 0));
        car_body->setActivationState(DISABLE_DEACTIVATION);
        world.addRigidBody(car_body.get());

        tuning.m_suspensionStiffness = 24;
        tuning.m_suspensionCompression = 4.4;
        tuning.m_suspensionDamping = 2.3;
        tuning.m_frictionSlip = 2.4;
        tuning.m_maxSuspensionForce = 9000;
        vehicle = std::make_unique<btRaycastVehicle>(
            tuning, car_body.get(), &raycaster);
        vehicle->setCoordinateSystem(0, 1, 2);
        world.addAction(vehicle.get());
        update_tree_collisions(0);
        constexpr btScalar wheel_radius = 0.38;
        constexpr btScalar suspension_rest = 0.38;
        const btVector3 wheel_direction(0, -1, 0);
        const btVector3 wheel_axle(-1, 0, 0);
        for (int front = 0; front < 2; ++front) {
            const btScalar z = front == 0 ? 1.45 : -1.45;
            for (int side = 0; side < 2; ++side) {
                const btScalar x = side == 0 ? -0.92 : 0.92;
                vehicle->addWheel(btVector3(x, 0.25, z),
                                  wheel_direction,
                                  wheel_axle,
                                  suspension_rest,
                                  wheel_radius,
                                  tuning,
                                  front == 0);
                btWheelInfo &wheel =
                    vehicle->getWheelInfo(vehicle->getNumWheels() - 1);
                wheel.m_rollInfluence = 0.08;
                wheel.m_wheelsDampingCompression = 4.4;
                wheel.m_wheelsDampingRelaxation = 2.3;
                wheel.m_suspensionStiffness = 24;
                wheel.m_frictionSlip = 2.4;
            }
        }
    }

    ~PhysicsState() {
        world.removeAction(vehicle.get());
        world.removeRigidBody(car_body.get());
        world.removeRigidBody(ground_body.get());
        for (auto &body : tree_bodies)
            world.removeRigidBody(body.get());
    }
};

BulletVehicleSimulator::BulletVehicleSimulator()
    : physics_(std::make_unique<PhysicsState>()) {}
BulletVehicleSimulator::~BulletVehicleSimulator() = default;

void BulletVehicleSimulator::set_controls(const SimulatorControls &controls) {
    controls_ = controls;
}
void BulletVehicleSimulator::set_gear(Gear gear) {
    if (std::abs(speed_mps_) < 0.6 || gear == Gear::neutral)
        gear_ = gear;
}
void BulletVehicleSimulator::toggle_headlights() {
    visual_.headlights = !visual_.headlights;
    if (!visual_.headlights)
        visual_.high_beam = false;
}
void BulletVehicleSimulator::toggle_high_beam() {
    visual_.high_beam = !visual_.high_beam;
    if (visual_.high_beam)
        visual_.headlights = true;
}
void BulletVehicleSimulator::toggle_battery_fault() {
    visual_.battery_fault = !visual_.battery_fault;
}
void BulletVehicleSimulator::toggle_tire_fault() {
    visual_.tire_fault = !visual_.tire_fault;
}
void BulletVehicleSimulator::toggle_drivetrain_fault() {
    visual_.drivetrain_fault = !visual_.drivetrain_fault;
}
void BulletVehicleSimulator::toggle_seat_belt() {
    seat_belt_fastened_ = !seat_belt_fastened_;
}
void BulletVehicleSimulator::toggle_lane_assist() {
    lane_assist_enabled_ = !lane_assist_enabled_;
}
void BulletVehicleSimulator::increase_cruise_speed() {
    if (cruise_target_kph_ < 0) {
        cruise_target_kph_ = visual_.speed_kph;
        cruise_control_active_ = true;
    }
    else
        cruise_target_kph_ = std::min(220.0, cruise_target_kph_ + 1.0);
}
void BulletVehicleSimulator::decrease_cruise_speed() {
    if (cruise_target_kph_ < 0) {
        cruise_target_kph_ = visual_.speed_kph;
        cruise_control_active_ = true;
    }
    else
        cruise_target_kph_ = std::max(0.0, cruise_target_kph_ - 1.0);
}
void BulletVehicleSimulator::toggle_cruise_control() {
    if (cruise_target_kph_ < 0)
        cruise_target_kph_ = visual_.speed_kph;
    cruise_control_active_ = !cruise_control_active_;
    cruise_integral_ = 0;
    previous_cruise_error_ = 0;
}
const SimulatorVisualState &BulletVehicleSimulator::visual_state() const {
    return visual_;
}

void BulletVehicleSimulator::advance(double dt) {
    dt = std::clamp(dt, 0.0, 0.05);
    if (dt <= 0)
        return;

    btRigidBody &body = *physics_->car_body;
    const btVector3 old_velocity = body.getLinearVelocity();
    const double old_speed = std::hypot(old_velocity.x(), old_velocity.z());
    const double driver_throttle = controls_.throttle ? 1.0 : 0.0;
    const double brake_target =
        controls_.brake || controls_.emergency_brake ? 1.0 : 0.0;
    if (brake_target > 0) {
        cruise_control_active_ = false;
        cruise_integral_ = 0;
    }
    const double brake_response = controls_.emergency_brake ? 10.0 : 2.5;
    brake_application_ += (brake_target - brake_application_) *
                          std::min(1.0, dt * brake_response);
    const bool can_drive = gear_ == Gear::drive || gear_ == Gear::reverse;
    const double direction = gear_ == Gear::reverse ? -1.0 : 1.0;
    double cruise_throttle = 0;
    if (cruise_control_active_ && gear_ == Gear::drive && brake_target == 0) {
        const double error = cruise_target_kph_ - old_speed * 3.6;
        cruise_integral_ =
            std::clamp(cruise_integral_ + error * dt, -20.0, 20.0);
        const double derivative = (error - previous_cruise_error_) / dt;
        const double derivative_term =
            std::clamp(derivative * 0.004, -0.2, 0.2);
        cruise_throttle = std::clamp(error * 0.06 + cruise_integral_ * 0.012 +
                                         derivative_term,
                                     0.0,
                                     1.0);
        previous_cruise_error_ = error;
    }
    else if (!cruise_control_active_) {
        cruise_integral_ = 0;
        previous_cruise_error_ = 0;
    }
    const double throttle = std::max(driver_throttle, cruise_throttle);
    const double power_limit = visual_.drivetrain_fault || visual_.battery_fault
                                   ? maximum_drive_power_w * 0.25
                                   : maximum_drive_power_w;
    const double traction_magnitude =
        can_drive && throttle > 0 && brake_target == 0
            ? throttle *
                  std::min(8500.0, power_limit / std::max(3.0, old_speed))
            : 0;

    // Driver steering overrides assistance. Otherwise lane assist aims at a
    // point 50 metres ahead on the road centerline.
    const double driver_steering = (controls_.steer_left ? 1.0 : 0.0) -
                                   (controls_.steer_right ? 1.0 : 0.0);
    double steer_target = driver_steering;
    if (lane_assist_enabled_ && driver_steering == 0) {
        const btTransform steering_transform = body.getWorldTransform();
        const btVector3 position = steering_transform.getOrigin();
        const btVector3 current_forward =
            steering_transform.getBasis() * btVector3(0, 0, 1);
        const double road_direction = current_forward.z() >= 0 ? 1.0 : -1.0;
        const double desired_heading =
            std::atan2(-position.x(), road_direction * 50.0);
        const double current_heading =
            std::atan2(current_forward.x(), current_forward.z());
        double heading_error = desired_heading - current_heading;
        while (heading_error > 3.141592653589793)
            heading_error -= 2 * 3.141592653589793;
        while (heading_error < -3.141592653589793)
            heading_error += 2 * 3.141592653589793;
        steer_target = std::clamp(heading_error * 1.8, -1.0, 1.0);
    }
    visual_.steering +=
        (steer_target - visual_.steering) * std::min(1.0, dt * 5.0);
    const btScalar steering_angle = (btScalar)(visual_.steering * 0.48);
    physics_->vehicle->setSteeringValue(steering_angle, 0);
    physics_->vehicle->setSteeringValue(steering_angle, 1);
    physics_->vehicle->applyEngineForce(
        (btScalar)(traction_magnitude * direction * 0.5), 2);
    physics_->vehicle->applyEngineForce(
        (btScalar)(traction_magnitude * direction * 0.5), 3);

    const double wheel_brake_impulse =
        brake_application_ * (controls_.emergency_brake
                                  ? emergency_wheel_brake_impulse
                                  : normal_wheel_brake_impulse);
    for (int wheel = 0; wheel < physics_->vehicle->getNumWheels(); ++wheel)
        physics_->vehicle->setBrake((btScalar)(wheel_brake_impulse * 0.25),
                                    wheel);

    const btVector3 horizontal_velocity(old_velocity.x(), 0, old_velocity.z());
    const double speed_abs = horizontal_velocity.length();
    const double regenerative_force = brake_application_ *
                                      maximum_regenerative_force_n *
                                      std::clamp(speed_abs / 2.5, 0.0, 1.0);
    body.clearForces();
    if (speed_abs > 0.02) {
        const double resistance =
            0.5 * air_density * drag_area * speed_abs * speed_abs +
            rolling_coefficient * vehicle_mass_kg * gravity;
        body.applyCentralForce(-horizontal_velocity.normalized() *
                               (resistance + regenerative_force));
    }
    physics_->world.stepSimulation(dt, 3, 1.0 / 120.0);

    btVector3 velocity = body.getLinearVelocity();
    speed_mps_ = std::hypot(velocity.x(), velocity.z());
    const btTransform transform = body.getWorldTransform();
    physics_->update_tree_collisions(transform.getOrigin().z());
    const btVector3 forward = transform.getBasis() * btVector3(0, 0, 1);
    const double forward_speed = velocity.dot(forward);
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
    if ((gear_ == Gear::drive && forward_speed < -0.2) ||
        (gear_ == Gear::reverse && forward_speed > 0.2)) {
        velocity.setX(0);
        velocity.setZ(0);
        body.setLinearVelocity(velocity);
        speed_mps_ = 0;
    }

    const double average_speed = (old_speed + speed_mps_) * 0.5;
    const double travelled_m = average_speed * dt;
    visual_.distance_m += travelled_m;
    trip_distance_km_ += travelled_m / 1000;
    const double mechanical_drive_w = traction_magnitude * average_speed;
    const double regenerative_w =
        regenerative_force > 0 && average_speed > 1
            ? std::min(maximum_regenerative_power_w,
                       regenerative_force * average_speed)
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

    visual_.world_position_x_m = transform.getOrigin().x();
    visual_.world_position_z_m = transform.getOrigin().z();
    visual_.lateral_position_m = visual_.world_position_x_m;
    visual_.heading_radians = std::atan2(forward.x(), forward.z());
    visual_.speed_kph = speed_mps_ * 3.6;
    const double load = mechanical_drive_w / maximum_drive_power_w;
    battery_temperature_c_ +=
        ((23 + load * 18) - battery_temperature_c_) * dt * 0.025;
    motor_temperature_c_ +=
        ((25 + load * 65) - motor_temperature_c_) * dt * 0.04;
    inverter_temperature_c_ +=
        ((25 + load * 48) - inverter_temperature_c_) * dt * 0.05;
}

VehicleData BulletVehicleSimulator::sample(double elapsed_seconds) {
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
    data.available_regen_power_kw = maximum_regenerative_power_w / 1000;
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
    data.warnings.lane_assist = lane_assist_enabled_;
    data.warnings.cruise_control = cruise_control_active_;
    data.cruise_control_target_kph = cruise_target_kph_;
    return data;
}
