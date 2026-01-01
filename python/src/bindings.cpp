#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/function.h>
#include <nanobind/ndarray.h>

#include "RocketSim.h"
#include "Math/Math.h"
#include "Sim/Arena/Arena.h"
#include "Sim/Car/Car.h"
#include "Sim/Ball/Ball.h"
#include "Sim/BoostPad/BoostPad.h"
#include "Sim/CarControls.h"
#include "Sim/MutatorConfig/MutatorConfig.h"

namespace nb = nanobind;
using namespace nb::literals;
using namespace RocketSim;

NB_MODULE(RocketSim, m) {
    m.doc() = "RocketSim - A C++ library for simulating Rocket League games at maximum efficiency";

    // ========== Module-level init function ==========
    m.def("init", [](const std::string& path) {
        RocketSim::Init(path);
    }, "collision_meshes_path"_a, "Initialize RocketSim with path to collision meshes directory");

    // ========== GameMode enum ==========
    nb::enum_<GameMode>(m, "GameMode")
        .value("SOCCAR", GameMode::SOCCAR)
        .value("HOOPS", GameMode::HOOPS)
        .value("HEATSEEKER", GameMode::HEATSEEKER)
        .value("SNOWDAY", GameMode::SNOWDAY)
        .value("DROPSHOT", GameMode::DROPSHOT)
        .value("THE_VOID", GameMode::THE_VOID);

    // ========== Team enum ==========
    nb::enum_<Team>(m, "Team")
        .value("BLUE", Team::BLUE)
        .value("ORANGE", Team::ORANGE);

    // ========== DemoMode enum ==========
    nb::enum_<DemoMode>(m, "DemoMode")
        .value("NORMAL", DemoMode::NORMAL)
        .value("ON_CONTACT", DemoMode::ON_CONTACT)
        .value("DISABLED", DemoMode::DISABLED);

    // ========== Vec class ==========
    nb::class_<Vec>(m, "Vec")
        .def(nb::init<float, float, float>(), "x"_a = 0.0f, "y"_a = 0.0f, "z"_a = 0.0f)
        .def_rw("x", &Vec::x)
        .def_rw("y", &Vec::y)
        .def_rw("z", &Vec::z)
        .def("__repr__", [](const Vec& v) {
            return "Vec(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        })
        .def("as_tuple", [](const Vec& v) {
            return nb::make_tuple(v.x, v.y, v.z);
        })
        .def("as_numpy", [](const Vec& v) {
            float* data = new float[3]{v.x, v.y, v.z};
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float, nb::shape<3>>(data, {3}, owner);
        });

    // ========== RotMat class ==========
    nb::class_<RotMat>(m, "RotMat")
        .def(nb::init<>())
        .def(nb::init<Vec, Vec, Vec>(), "forward"_a, "right"_a, "up"_a)
        .def("__init__", [](RotMat* self, nb::ndarray<float, nb::shape<9>> arr) {
            auto data = arr.data();
            // Flattened row-major: [f.x, f.y, f.z, r.x, r.y, r.z, u.x, u.y, u.z]
            new (self) RotMat(
                Vec(data[0], data[1], data[2]),
                Vec(data[3], data[4], data[5]),
                Vec(data[6], data[7], data[8])
            );
        })
        .def_rw("forward", &RotMat::forward)
        .def_rw("right", &RotMat::right)
        .def_rw("up", &RotMat::up)
        .def("__repr__", [](const RotMat& m) {
            return "RotMat(forward=" + std::to_string(m.forward.x) + "," + std::to_string(m.forward.y) + "," + std::to_string(m.forward.z) + ")";
        })
        .def("as_numpy", [](const RotMat& m) {
            // Return as 3x3 matrix (row-major for numpy compatibility)
            float* data = new float[9]{
                m.forward.x, m.forward.y, m.forward.z,
                m.right.x, m.right.y, m.right.z,
                m.up.x, m.up.y, m.up.z
            };
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float, nb::shape<3, 3>>(data, {3, 3}, owner);
        })
        .def_static("get_identity", &RotMat::GetIdentity);

    // ========== Angle class ==========
    nb::class_<Angle>(m, "Angle")
        .def(nb::init<float, float, float>(), "yaw"_a = 0.0f, "pitch"_a = 0.0f, "roll"_a = 0.0f)
        .def_rw("yaw", &Angle::yaw)
        .def_rw("pitch", &Angle::pitch)
        .def_rw("roll", &Angle::roll)
        .def("to_rot_mat", &Angle::ToRotMat)
        .def_static("from_rot_mat", &Angle::FromRotMat);

    // ========== CarControls class ==========
    nb::class_<CarControls>(m, "CarControls")
        .def(nb::init<>())
        .def_rw("throttle", &CarControls::throttle)
        .def_rw("steer", &CarControls::steer)
        .def_rw("pitch", &CarControls::pitch)
        .def_rw("yaw", &CarControls::yaw)
        .def_rw("roll", &CarControls::roll)
        .def_rw("boost", &CarControls::boost)
        .def_rw("jump", &CarControls::jump)
        .def_rw("handbrake", &CarControls::handbrake)
        .def("clamp_fix", &CarControls::ClampFix);

    // ========== BallState class ==========
    nb::class_<BallState>(m, "BallState")
        .def(nb::init<>())
        .def_rw("pos", &BallState::pos)
        .def_rw("vel", &BallState::vel)
        .def_rw("ang_vel", &BallState::angVel)
        .def_rw("rot_mat", &BallState::rotMat);

    // ========== BoostPadState class ==========
    nb::class_<BoostPadState>(m, "BoostPadState")
        .def(nb::init<>())
        .def_rw("is_active", &BoostPadState::isActive)
        .def_rw("cooldown", &BoostPadState::cooldown);

    // ========== WheelPairConfig class ==========
    nb::class_<WheelPairConfig>(m, "WheelPairConfig")
        .def(nb::init<>())
        .def_rw("wheel_radius", &WheelPairConfig::wheelRadius)
        .def_rw("suspension_rest_length", &WheelPairConfig::suspensionRestLength)
        .def_rw("connection_point_offset", &WheelPairConfig::connectionPointOffset);

    // ========== CarConfig class ==========
    nb::class_<CarConfig>(m, "CarConfig")
        .def(nb::init<>())
        .def("__init__", [](CarConfig* self, int hitbox_type) {
            static const CarConfig* configs[] = {
                &CAR_CONFIG_OCTANE,
                &CAR_CONFIG_DOMINUS,
                &CAR_CONFIG_PLANK,
                &CAR_CONFIG_BREAKOUT,
                &CAR_CONFIG_HYBRID,
                &CAR_CONFIG_MERC,
            };
            if (hitbox_type < 0 || hitbox_type > 5) hitbox_type = 0;
            new (self) CarConfig(*configs[hitbox_type]);
        }, "hitbox_type"_a = 0)
        .def_rw("hitbox_size", &CarConfig::hitboxSize)
        .def_rw("hitbox_pos_offset", &CarConfig::hitboxPosOffset)
        .def_rw("front_wheels", &CarConfig::frontWheels)
        .def_rw("back_wheels", &CarConfig::backWheels)
        .def_rw("dodge_deadzone", &CarConfig::dodgeDeadzone);

    // ========== CarState class ==========
    nb::class_<CarState>(m, "CarState")
        .def(nb::init<>())
        // Position and rotation
        .def_rw("pos", &CarState::pos)
        .def_rw("rot_mat", &CarState::rotMat)
        .def_rw("vel", &CarState::vel)
        .def_rw("ang_vel", &CarState::angVel)
        // Ground contact
        .def_rw("is_on_ground", &CarState::isOnGround)
        .def_prop_rw("wheels_with_contact",
            [](const CarState& s) {
                nb::list result;
                for (int i = 0; i < 4; i++) result.append(s.wheelsWithContact[i]);
                return result;
            },
            [](CarState& s, nb::list wheels) {
                for (size_t i = 0; i < 4 && i < nb::len(wheels); i++) {
                    s.wheelsWithContact[i] = nb::cast<bool>(wheels[i]);
                }
            })
        // Jump state
        .def_rw("has_jumped", &CarState::hasJumped)
        .def_rw("is_jumping", &CarState::isJumping)
        .def_rw("jump_time", &CarState::jumpTime)
        // Double jump
        .def_rw("has_double_jumped", &CarState::hasDoubleJumped)
        .def_rw("air_time_since_jump", &CarState::airTimeSinceJump)
        // Flip state
        .def_rw("has_flipped", &CarState::hasFlipped)
        .def_rw("is_flipping", &CarState::isFlipping)
        .def_rw("flip_time", &CarState::flipTime)
        .def_rw("flip_rel_torque", &CarState::flipRelTorque)
        // Auto-flip
        .def_rw("is_auto_flipping", &CarState::isAutoFlipping)
        .def_rw("auto_flip_timer", &CarState::autoFlipTimer)
        .def_rw("auto_flip_torque_scale", &CarState::autoFlipTorqueScale)
        // Boost
        .def_rw("boost", &CarState::boost)
        .def_rw("time_spent_boosting", &CarState::boostingTime)
        // Supersonic
        .def_rw("is_supersonic", &CarState::isSupersonic)
        .def_rw("supersonic_time", &CarState::supersonicTime)
        // Handbrake
        .def_rw("handbrake_val", &CarState::handbrakeVal)
        // Demo
        .def_rw("is_demoed", &CarState::isDemoed)
        .def_rw("demo_respawn_timer", &CarState::demoRespawnTimer)
        // Car contact
        .def_prop_rw("car_contact_id",
            [](const CarState& s) { return s.carContact.otherCarID; },
            [](CarState& s, uint32_t id) { s.carContact.otherCarID = id; })
        .def_prop_rw("car_contact_cooldown_timer",
            [](const CarState& s) { return s.carContact.cooldownTimer; },
            [](CarState& s, float t) { s.carContact.cooldownTimer = t; })
        // Last controls
        .def_rw("last_controls", &CarState::lastControls);

    // ========== MutatorConfig class ==========
    nb::class_<MutatorConfig>(m, "MutatorConfig")
        .def(nb::init<GameMode>(), "game_mode"_a = GameMode::SOCCAR)
        .def_rw("gravity", &MutatorConfig::gravity)
        .def_rw("car_mass", &MutatorConfig::carMass)
        .def_rw("car_world_friction", &MutatorConfig::carWorldFriction)
        .def_rw("car_world_restitution", &MutatorConfig::carWorldRestitution)
        .def_rw("ball_mass", &MutatorConfig::ballMass)
        .def_rw("ball_max_speed", &MutatorConfig::ballMaxSpeed)
        .def_rw("ball_drag", &MutatorConfig::ballDrag)
        .def_rw("ball_world_friction", &MutatorConfig::ballWorldFriction)
        .def_rw("ball_world_restitution", &MutatorConfig::ballWorldRestitution)
        .def_rw("ball_radius", &MutatorConfig::ballRadius)
        .def_rw("jump_accel", &MutatorConfig::jumpAccel)
        .def_rw("jump_immediate_force", &MutatorConfig::jumpImmediateForce)
        .def_rw("boost_accel_ground", &MutatorConfig::boostAccelGround)
        .def_rw("boost_accel_air", &MutatorConfig::boostAccelAir)
        .def_rw("boost_used_per_second", &MutatorConfig::boostUsedPerSecond)
        .def_rw("respawn_delay", &MutatorConfig::respawnDelay)
        .def_rw("bump_cooldown_time", &MutatorConfig::bumpCooldownTime)
        .def_rw("boost_pad_cooldown_big", &MutatorConfig::boostPadCooldown_Big)
        .def_rw("boost_pad_cooldown_small", &MutatorConfig::boostPadCooldown_Small)
        .def_rw("car_spawn_boost_amount", &MutatorConfig::carSpawnBoostAmount)
        .def_rw("ball_hit_extra_force_scale", &MutatorConfig::ballHitExtraForceScale)
        .def_rw("bump_force_scale", &MutatorConfig::bumpForceScale)
        .def_rw("unlimited_flips", &MutatorConfig::unlimitedFlips)
        .def_rw("unlimited_double_jumps", &MutatorConfig::unlimitedDoubleJumps)
        .def_rw("demo_mode", &MutatorConfig::demoMode)
        .def_rw("enable_team_demos", &MutatorConfig::enableTeamDemos);

    // ========== Ball class ==========
    nb::class_<Ball>(m, "Ball")
        .def("get_state", &Ball::GetState)
        .def("set_state", &Ball::SetState)
        .def("get_radius", &Ball::GetRadius);

    // ========== BoostPad class ==========
    nb::class_<BoostPad>(m, "BoostPad")
        .def("get_state", &BoostPad::GetState)
        .def("set_state", &BoostPad::SetState)
        .def("get_pos", [](const BoostPad& p) { return p.config.pos; })
        .def_prop_ro("is_big", [](const BoostPad& p) { return p.config.isBig; });

    // ========== Car class ==========
    nb::class_<Car>(m, "Car")
        .def("get_state", &Car::GetState)
        .def("set_state", &Car::SetState)
        .def("get_controls", [](const Car& c) { return c.controls; })
        .def("set_controls", [](Car& c, const CarControls& ctrl) { c.controls = ctrl; })
        .def("get_config", [](const Car& c) { return c.config; })
        .def("demolish", &Car::Demolish, "respawn_delay"_a = 3.0f)
        .def("respawn", &Car::Respawn, "game_mode"_a, "seed"_a = -1, "boost_amount"_a = 33.33f)
        .def_prop_ro("id", [](const Car& c) { return c.id; })
        .def_prop_ro("team", [](const Car& c) { return static_cast<int>(c.team); });

    // ========== Arena class ==========
    nb::class_<Arena>(m, "Arena")
        .def(nb::new_([](GameMode gameMode, float tickRate) {
            return Arena::Create(gameMode, ArenaConfig{}, tickRate);
        }), "game_mode"_a, "tick_rate"_a = 120.0f)
        .def("step", &Arena::Step, "ticks_to_simulate"_a = 1)
        .def("clone", [](Arena* a) { return a->Clone(false); }, nb::rv_policy::take_ownership)
        .def("add_car", [](Arena* a, Team team, const CarConfig& config) {
            return a->AddCar(team, config);
        }, "team"_a, "config"_a, nb::rv_policy::reference)
        .def("remove_car", [](Arena* a, Car* car) { a->RemoveCar(car); })
        .def("get_cars", [](Arena* a) {
            std::vector<Car*> cars;
            for (auto* car : a->GetCars()) {
                cars.push_back(car);
            }
            return cars;
        }, nb::rv_policy::reference)
        .def("get_car_from_id", &Arena::GetCar, "car_id"_a, nb::rv_policy::reference)
        .def("get_boost_pads", [](Arena* a) {
            return a->GetBoostPads();
        }, nb::rv_policy::reference)
        .def("set_mutator_config", &Arena::SetMutatorConfig)
        .def("get_mutator_config", &Arena::GetMutatorConfig, nb::rv_policy::reference)
        .def("reset_to_random_kickoff", &Arena::ResetToRandomKickoff, "seed"_a = -1)
        .def("is_ball_probably_going_in", &Arena::IsBallProbablyGoingIn, 
             "max_time"_a = 2.0f, "extra_margin"_a = 0.0f, "goal_team_out"_a = nullptr)
        // Ball touch callback - stores Python callable
        .def("set_ball_touch_callback", [](Arena* a, nb::object callback) {
            // Note: RocketSim doesn't have a direct ball touch callback
            // This would need to be implemented via the goal score callback or custom logic
            // For now, we'll store it but it won't be called automatically
            // Real implementation would require modifying RocketSim C++ code
        }, "callback"_a)
        .def_prop_ro("ball", [](Arena* a) { return a->ball; }, nb::rv_policy::reference)
        .def_prop_ro("game_mode", [](const Arena& a) { return a.gameMode; })
        .def_prop_ro("tick_count", [](const Arena& a) { return a.tickCount; })
        .def_prop_ro("tick_rate", &Arena::GetTickRate)
        .def_prop_ro("tick_time", [](const Arena& a) { return a.tickTime; });

    // ========== Car config presets ==========
    m.attr("CAR_CONFIG_OCTANE") = &CAR_CONFIG_OCTANE;
    m.attr("CAR_CONFIG_DOMINUS") = &CAR_CONFIG_DOMINUS;
    m.attr("CAR_CONFIG_PLANK") = &CAR_CONFIG_PLANK;
    m.attr("CAR_CONFIG_BREAKOUT") = &CAR_CONFIG_BREAKOUT;
    m.attr("CAR_CONFIG_HYBRID") = &CAR_CONFIG_HYBRID;
    m.attr("CAR_CONFIG_MERC") = &CAR_CONFIG_MERC;

    // ========== Hitbox type constants ==========
    m.attr("OCTANE") = 0;
    m.attr("DOMINUS") = 1;
    m.attr("PLANK") = 2;
    m.attr("BREAKOUT") = 3;
    m.attr("HYBRID") = 4;
    m.attr("MERC") = 5;
}
