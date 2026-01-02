#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/ndarray.h>

#include "RocketSim.h"
#include "Math/Math.h"
#include "Sim/Arena/Arena.h"
#include "Sim/Car/Car.h"
#include "Sim/Ball/Ball.h"
#include "Sim/BoostPad/BoostPad.h"
#include "Sim/CarControls.h"
#include "Sim/MutatorConfig/MutatorConfig.h"

#include <unordered_map>
#include <memory>
#include <cstring>

namespace nb = nanobind;
using namespace nb::literals;
using namespace RocketSim;

// ============================================================================
// GymState - Efficient pre-allocated state container for RLGym
// ============================================================================
struct GymState {
    // Ball state: pos(3) + vel(3) + ang_vel(3) = 9 floats
    // Plus rotation matrix: 9 floats = 18 total
    static constexpr size_t BALL_STATE_SIZE = 18;
    
    // Car state: pos(3) + vel(3) + ang_vel(3) + rot_mat(9) + boost(1) + 
    // on_ground(1) + has_jumped(1) + has_double_jumped(1) + has_flipped(1) + 
    // is_demoed(1) + on_wall(1) + supersonic(1) = 25 floats
    static constexpr size_t CAR_STATE_SIZE = 25;
    
    std::vector<float> ball_data;
    std::vector<float> cars_data;
    std::vector<float> pads_data;
    std::vector<float> game_data; // [blue_score, orange_score, tick_count]
    
    size_t num_cars = 0;
    size_t num_pads = 0;
    
    void resize(size_t cars, size_t pads) {
        if (cars != num_cars) {
            cars_data.resize(cars * CAR_STATE_SIZE);
            num_cars = cars;
        }
        if (pads != num_pads) {
            pads_data.resize(pads);
            num_pads = pads;
        }
        if (ball_data.size() != BALL_STATE_SIZE) {
            ball_data.resize(BALL_STATE_SIZE);
        }
        if (game_data.size() != 3) {
            game_data.resize(3);
        }
    }
};

// ============================================================================
// ArenaWrapper - Wraps Arena with Python callbacks and stat tracking
// ============================================================================
struct ArenaWrapper {
    std::unique_ptr<Arena, void(*)(Arena*)> arena;
    
    // Score tracking
    int blue_score = 0;
    int orange_score = 0;
    
    // Per-car stats (indexed by car ID)
    struct CarStats {
        int goals = 0;
        int demos = 0;
        int boost_pickups = 0;
    };
    std::unordered_map<uint32_t, CarStats> car_stats;
    
    // Python callbacks (stored to prevent GC)
    // Each callback stores (callback, data) tuple
    nb::object goal_score_callback;
    nb::object goal_score_data;
    nb::object car_bump_callback;
    nb::object car_bump_data;
    nb::object boost_pickup_callback;
    nb::object boost_pickup_data;
    nb::object ball_touch_callback;
    nb::object ball_touch_data;
    
    // Reusable gym state buffer
    GymState gym_state;
    
    // Track last gym state tick for ball_touched detection
    uint64_t last_gym_state_tick = 0;
    
    ArenaWrapper(GameMode mode, float tick_rate) 
        : arena(nullptr, [](Arena* a) { if (a) delete a; })
    {
        // Validate tick rate (RL uses 120, but allow reasonable range)
        if (tick_rate < 15.0f || tick_rate > 120.0f) {
            throw std::invalid_argument("tick_rate must be between 15 and 120");
        }
        
        arena.reset(Arena::Create(mode, ArenaConfig{}, tick_rate));
        setup_callbacks();
    }
    
    void setup_callbacks() {
        // Goal callback - only for non-THE_VOID modes
        if (arena->gameMode != GameMode::THE_VOID) {
            arena->SetGoalScoreCallback([](Arena* a, Team team, void* userInfo) {
                auto* self = static_cast<ArenaWrapper*>(userInfo);
                if (team == Team::BLUE) {
                    self->blue_score++;
                } else {
                    self->orange_score++;
                }
                // Call Python callback if set
                if (self->goal_score_callback) {
                    nb::gil_scoped_acquire gil;
                    try {
                        self->goal_score_callback(
                            "arena"_a = nb::cast(self, nb::rv_policy::reference),
                            "scoring_team"_a = team,
                            "data"_a = self->goal_score_data
                        );
                    } catch (...) {}
                }
            }, this);
        }
        
        // Car bump/demo callback
        arena->SetCarBumpCallback([](Arena* a, Car* bumper, Car* victim, bool isDemo, void* userInfo) {
            auto* self = static_cast<ArenaWrapper*>(userInfo);
            
            if (isDemo) {
                self->car_stats[bumper->id].demos++;
            }
            
            // Call bump callback if set
            if (self->car_bump_callback) {
                nb::gil_scoped_acquire gil;
                try {
                    self->car_bump_callback(
                        "arena"_a = nb::cast(self, nb::rv_policy::reference),
                        "bumper"_a = nb::cast(bumper, nb::rv_policy::reference),
                        "victim"_a = nb::cast(victim, nb::rv_policy::reference),
                        "is_demo"_a = isDemo,
                        "data"_a = self->car_bump_data
                    );
                } catch (...) {}
            }
        }, this);
        
        // Boost pickup callback
        // Only set for non-THE_VOID modes
        if (arena->gameMode != GameMode::THE_VOID) {
            arena->SetBoostPickupCallback([](Arena* a, Car* car, BoostPad* pad, void* userInfo) {
                auto* self = static_cast<ArenaWrapper*>(userInfo);
                
                // Track boost pickups
                self->car_stats[car->id].boost_pickups++;
                
                // Call Python callback if set
                if (self->boost_pickup_callback) {
                    nb::gil_scoped_acquire gil;
                    try {
                        self->boost_pickup_callback(
                            "arena"_a = nb::cast(self, nb::rv_policy::reference),
                            "car"_a = nb::cast(car, nb::rv_policy::reference),
                            "boost_pad"_a = nb::cast(pad, nb::rv_policy::reference),
                            "data"_a = self->boost_pickup_data
                        );
                    } catch (...) {}
                }
            }, this);
        }
    }
    
    Car* add_car(Team team, const CarConfig& config) {
        Car* car = arena->AddCar(team, config);
        car_stats[car->id] = CarStats{};
        return car;
    }
    
    void remove_car(Car* car) {
        car_stats.erase(car->id);
        arena->RemoveCar(car);
    }
    
    void step(int ticks = 1) {
        // Boost pickups are now tracked via C++ callback
        arena->Step(ticks);
    }
    
    void reset_to_random_kickoff(int seed = -1) {
        arena->ResetToRandomKickoff(seed);
        blue_score = 0;
        orange_score = 0;
        for (auto& [id, stats] : car_stats) {
            stats = CarStats{};
        }
    }
    
    ArenaWrapper* clone(bool copy_callbacks = false) {
        auto* cloned = new ArenaWrapper(arena->gameMode, arena->GetTickRate());
        // Copy the arena state
        delete cloned->arena.release();
        cloned->arena = std::unique_ptr<Arena, void(*)(Arena*)>(
            arena->Clone(copy_callbacks), [](Arena* a) { delete a; }
        );
        cloned->setup_callbacks();
        cloned->blue_score = blue_score;
        cloned->orange_score = orange_score;
        cloned->car_stats = car_stats;
        
        // Copy Python callbacks if requested
        if (copy_callbacks) {
            cloned->goal_score_callback = goal_score_callback;
            cloned->goal_score_data = goal_score_data;
            cloned->car_bump_callback = car_bump_callback;
            cloned->car_bump_data = car_bump_data;
            cloned->boost_pickup_callback = boost_pickup_callback;
            cloned->boost_pickup_data = boost_pickup_data;
            cloned->ball_touch_callback = ball_touch_callback;
            cloned->ball_touch_data = ball_touch_data;
            // Re-setup ball touch callback on clone if it was set
            if (cloned->ball_touch_callback) {
                cloned->arena->SetBallTouchCallback([](Arena* a, Car* car, void* userInfo) {
                    auto* self = static_cast<ArenaWrapper*>(userInfo);
                    if (self->ball_touch_callback) {
                        nb::gil_scoped_acquire gil;
                        try {
                            self->ball_touch_callback(
                                "arena"_a = nb::cast(self, nb::rv_policy::reference),
                                "car"_a = nb::cast(car, nb::rv_policy::reference),
                                "data"_a = self->ball_touch_data
                            );
                        } catch (...) {}
                    }
                }, cloned);
            }
        }
        return cloned;
    }
    
    // ========================================================================
    // Efficient gym state getters - return numpy arrays with minimal overhead
    // ========================================================================
    
    nb::ndarray<nb::numpy, float> get_ball_state() {
        BallState bs = arena->ball->GetState();
        float* data = new float[18];
        
        // Position
        data[0] = bs.pos.x; data[1] = bs.pos.y; data[2] = bs.pos.z;
        // Velocity
        data[3] = bs.vel.x; data[4] = bs.vel.y; data[5] = bs.vel.z;
        // Angular velocity
        data[6] = bs.angVel.x; data[7] = bs.angVel.y; data[8] = bs.angVel.z;
        // Rotation matrix (flattened row-major)
        data[9] = bs.rotMat.forward.x; data[10] = bs.rotMat.forward.y; data[11] = bs.rotMat.forward.z;
        data[12] = bs.rotMat.right.x; data[13] = bs.rotMat.right.y; data[14] = bs.rotMat.right.z;
        data[15] = bs.rotMat.up.x; data[16] = bs.rotMat.up.y; data[17] = bs.rotMat.up.z;
        
        nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
        return nb::ndarray<nb::numpy, float>(data, {18}, owner);
    }
    
    nb::ndarray<nb::numpy, float> get_car_state(Car* car) {
        CarState cs = car->GetState();
        float* data = new float[26];
        
        // Position
        data[0] = cs.pos.x; data[1] = cs.pos.y; data[2] = cs.pos.z;
        // Velocity
        data[3] = cs.vel.x; data[4] = cs.vel.y; data[5] = cs.vel.z;
        // Angular velocity
        data[6] = cs.angVel.x; data[7] = cs.angVel.y; data[8] = cs.angVel.z;
        // Rotation matrix
        data[9] = cs.rotMat.forward.x; data[10] = cs.rotMat.forward.y; data[11] = cs.rotMat.forward.z;
        data[12] = cs.rotMat.right.x; data[13] = cs.rotMat.right.y; data[14] = cs.rotMat.right.z;
        data[15] = cs.rotMat.up.x; data[16] = cs.rotMat.up.y; data[17] = cs.rotMat.up.z;
        // State flags
        data[18] = cs.boost;
        data[19] = cs.isOnGround ? 1.0f : 0.0f;
        data[20] = cs.hasJumped ? 1.0f : 0.0f;
        data[21] = cs.hasDoubleJumped ? 1.0f : 0.0f;
        data[22] = cs.hasFlipped ? 1.0f : 0.0f;
        data[23] = cs.isDemoed ? 1.0f : 0.0f;
        data[24] = cs.isSupersonic ? 1.0f : 0.0f;
        // Ball touched since last gym state
        bool ball_touched = cs.ballHitInfo.isValid && 
                           cs.ballHitInfo.tickCountWhenHit >= last_gym_state_tick;
        data[25] = ball_touched ? 1.0f : 0.0f;
        
        nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
        return nb::ndarray<nb::numpy, float>(data, {26}, owner);
    }
    
    nb::ndarray<nb::numpy, float> get_cars_state() {
        const auto& cars = arena->GetCars();
        size_t n = cars.size();
        float* data = new float[n * 26];
        
        size_t i = 0;
        for (Car* car : cars) {
            CarState cs = car->GetState();
            float* row = data + i * 26;
            
            row[0] = cs.pos.x; row[1] = cs.pos.y; row[2] = cs.pos.z;
            row[3] = cs.vel.x; row[4] = cs.vel.y; row[5] = cs.vel.z;
            row[6] = cs.angVel.x; row[7] = cs.angVel.y; row[8] = cs.angVel.z;
            row[9] = cs.rotMat.forward.x; row[10] = cs.rotMat.forward.y; row[11] = cs.rotMat.forward.z;
            row[12] = cs.rotMat.right.x; row[13] = cs.rotMat.right.y; row[14] = cs.rotMat.right.z;
            row[15] = cs.rotMat.up.x; row[16] = cs.rotMat.up.y; row[17] = cs.rotMat.up.z;
            row[18] = cs.boost;
            row[19] = cs.isOnGround ? 1.0f : 0.0f;
            row[20] = cs.hasJumped ? 1.0f : 0.0f;
            row[21] = cs.hasDoubleJumped ? 1.0f : 0.0f;
            row[22] = cs.hasFlipped ? 1.0f : 0.0f;
            row[23] = cs.isDemoed ? 1.0f : 0.0f;
            row[24] = cs.isSupersonic ? 1.0f : 0.0f;
            // Ball touched since last gym state
            bool ball_touched = cs.ballHitInfo.isValid && 
                               cs.ballHitInfo.tickCountWhenHit >= last_gym_state_tick;
            row[25] = ball_touched ? 1.0f : 0.0f;
            i++;
        }
        
        nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
        return nb::ndarray<nb::numpy, float>(data, {n, 26}, owner);
    }
    
    nb::ndarray<nb::numpy, float> get_pads_state() {
        const auto& pads = arena->GetBoostPads();
        size_t n = pads.size();
        float* data = new float[n];
        
        for (size_t i = 0; i < n; i++) {
            data[i] = pads[i]->GetState().isActive ? 1.0f : 0.0f;
        }
        
        nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
        return nb::ndarray<nb::numpy, float>(data, {n}, owner);
    }
    
    // Full gym state in one call - most efficient for RLGym
    nb::dict get_gym_state() {
        nb::dict result;
        result["ball"] = get_ball_state();
        result["cars"] = get_cars_state();
        result["pads"] = get_pads_state();
        result["blue_score"] = blue_score;
        result["orange_score"] = orange_score;
        result["tick_count"] = arena->tickCount;
        
        // Car IDs in same order as cars array
        nb::list car_ids;
        for (Car* car : arena->GetCars()) {
            car_ids.append(car->id);
        }
        result["car_ids"] = car_ids;
        
        // Car teams in same order
        nb::list car_teams;
        for (Car* car : arena->GetCars()) {
            car_teams.append(static_cast<int>(car->team));
        }
        result["car_teams"] = car_teams;
        
        // Update last_gym_state_tick for next call's ball_touched detection
        last_gym_state_tick = arena->tickCount;
        
        return result;
    }
};

// ============================================================================
// Module definition
// ============================================================================
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
        // Rich comparison operators
        .def("__eq__", [](const Vec& a, const Vec& b) {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        })
        .def("__ne__", [](const Vec& a, const Vec& b) {
            return a.x != b.x || a.y != b.y || a.z != b.z;
        })
        .def("__lt__", [](const Vec& a, const Vec& b) {
            return std::make_tuple(a.x, a.y, a.z) < std::make_tuple(b.x, b.y, b.z);
        })
        .def("__le__", [](const Vec& a, const Vec& b) {
            return std::make_tuple(a.x, a.y, a.z) <= std::make_tuple(b.x, b.y, b.z);
        })
        .def("__gt__", [](const Vec& a, const Vec& b) {
            return std::make_tuple(a.x, a.y, a.z) > std::make_tuple(b.x, b.y, b.z);
        })
        .def("__ge__", [](const Vec& a, const Vec& b) {
            return std::make_tuple(a.x, a.y, a.z) >= std::make_tuple(b.x, b.y, b.z);
        })
        .def("__hash__", [](const Vec& v) {
            // Use tuple hashing for Vec
            return nb::hash(nb::make_tuple(v.x, v.y, v.z));
        })
        .def("as_tuple", [](const Vec& v) {
            return nb::make_tuple(v.x, v.y, v.z);
        })
        .def("as_numpy", [](const Vec& v) {
            float* data = new float[3]{v.x, v.y, v.z};
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float, nb::shape<3>>(data, {3}, owner);
        })
        // Pickle support
        .def("__getstate__", [](const Vec& v) {
            return nb::make_tuple(v.x, v.y, v.z);
        })
        .def("__setstate__", [](Vec& v, nb::tuple t) {
            new (&v) Vec(nb::cast<float>(t[0]), nb::cast<float>(t[1]), nb::cast<float>(t[2]));
        });

    // ========== RotMat class ==========
    nb::class_<RotMat>(m, "RotMat")
        .def(nb::init<>())
        .def(nb::init<Vec, Vec, Vec>(), "forward"_a, "right"_a, "up"_a)
        .def("__init__", [](RotMat* self, 
                           std::optional<Vec> forward,
                           std::optional<Vec> right,
                           std::optional<Vec> up) {
            new (self) RotMat();
            if (forward) self->forward = *forward;
            if (right) self->right = *right;
            if (up) self->up = *up;
        }, "forward"_a = std::nullopt, "right"_a = std::nullopt, "up"_a = std::nullopt)
        .def_rw("forward", &RotMat::forward)
        .def_rw("right", &RotMat::right)
        .def_rw("up", &RotMat::up)
        .def("__repr__", [](const RotMat& m) {
            return "RotMat(forward=" + std::to_string(m.forward.x) + "," + std::to_string(m.forward.y) + "," + std::to_string(m.forward.z) + ")";
        })
        .def("as_numpy", [](const RotMat& m) {
            float* data = new float[9]{
                m.forward.x, m.forward.y, m.forward.z,
                m.right.x, m.right.y, m.right.z,
                m.up.x, m.up.y, m.up.z
            };
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float, nb::shape<3, 3>>(data, {3, 3}, owner);
        })
        .def("as_angle", [](const RotMat& m) {
            return Angle::FromRotMat(m);
        })
        .def_static("get_identity", &RotMat::GetIdentity)
        // Pickle support
        .def("__getstate__", [](const RotMat& m) {
            return nb::make_tuple(
                nb::make_tuple(m.forward.x, m.forward.y, m.forward.z),
                nb::make_tuple(m.right.x, m.right.y, m.right.z),
                nb::make_tuple(m.up.x, m.up.y, m.up.z)
            );
        })
        .def("__setstate__", [](RotMat& m, nb::tuple t) {
            auto fwd = nb::cast<nb::tuple>(t[0]);
            auto right = nb::cast<nb::tuple>(t[1]);
            auto up = nb::cast<nb::tuple>(t[2]);
            new (&m) RotMat(
                Vec(nb::cast<float>(fwd[0]), nb::cast<float>(fwd[1]), nb::cast<float>(fwd[2])),
                Vec(nb::cast<float>(right[0]), nb::cast<float>(right[1]), nb::cast<float>(right[2])),
                Vec(nb::cast<float>(up[0]), nb::cast<float>(up[1]), nb::cast<float>(up[2]))
            );
        });

    // ========== Angle class ==========
    nb::class_<Angle>(m, "Angle")
        .def(nb::init<>())
        .def(nb::init<float, float, float>(), "yaw"_a, "pitch"_a, "roll"_a)
        .def("__init__", [](Angle* self,
                           std::optional<float> yaw,
                           std::optional<float> pitch,
                           std::optional<float> roll) {
            new (self) Angle();
            if (yaw) self->yaw = *yaw;
            if (pitch) self->pitch = *pitch;
            if (roll) self->roll = *roll;
        }, "yaw"_a = std::nullopt, "pitch"_a = std::nullopt, "roll"_a = std::nullopt)
        .def_rw("yaw", &Angle::yaw)
        .def_rw("pitch", &Angle::pitch)
        .def_rw("roll", &Angle::roll)
        .def("to_rot_mat", &Angle::ToRotMat)
        .def("as_rot_mat", &Angle::ToRotMat)  // Alias for compatibility
        .def_static("from_rot_mat", &Angle::FromRotMat)
        // Pickle support
        .def("__getstate__", [](const Angle& a) {
            return nb::make_tuple(a.yaw, a.pitch, a.roll);
        })
        .def("__setstate__", [](Angle& a, nb::tuple t) {
            new (&a) Angle(nb::cast<float>(t[0]), nb::cast<float>(t[1]), nb::cast<float>(t[2]));
        });

    // ========== CarControls class ==========
    nb::class_<CarControls>(m, "CarControls")
        .def(nb::init<>())
        .def("__init__", [](CarControls* self,
                           std::optional<float> throttle,
                           std::optional<float> steer,
                           std::optional<float> pitch,
                           std::optional<float> yaw,
                           std::optional<float> roll,
                           std::optional<bool> boost,
                           std::optional<bool> jump,
                           std::optional<bool> handbrake) {
            new (self) CarControls();
            if (throttle) self->throttle = *throttle;
            if (steer) self->steer = *steer;
            if (pitch) self->pitch = *pitch;
            if (yaw) self->yaw = *yaw;
            if (roll) self->roll = *roll;
            if (boost) self->boost = *boost;
            if (jump) self->jump = *jump;
            if (handbrake) self->handbrake = *handbrake;
        }, "throttle"_a = std::nullopt, "steer"_a = std::nullopt, "pitch"_a = std::nullopt,
           "yaw"_a = std::nullopt, "roll"_a = std::nullopt, "boost"_a = std::nullopt,
           "jump"_a = std::nullopt, "handbrake"_a = std::nullopt)
        .def_rw("throttle", &CarControls::throttle)
        .def_rw("steer", &CarControls::steer)
        .def_rw("pitch", &CarControls::pitch)
        .def_rw("yaw", &CarControls::yaw)
        .def_rw("roll", &CarControls::roll)
        .def_rw("boost", &CarControls::boost)
        .def_rw("jump", &CarControls::jump)
        .def_rw("handbrake", &CarControls::handbrake)
        .def("clamp_fix", &CarControls::ClampFix)
        // Pickle support
        .def("__getstate__", [](const CarControls& c) {
            return nb::make_tuple(c.throttle, c.steer, c.pitch, c.yaw, c.roll, c.boost, c.jump, c.handbrake);
        })
        .def("__setstate__", [](CarControls& c, nb::tuple t) {
            new (&c) CarControls();
            c.throttle = nb::cast<float>(t[0]);
            c.steer = nb::cast<float>(t[1]);
            c.pitch = nb::cast<float>(t[2]);
            c.yaw = nb::cast<float>(t[3]);
            c.roll = nb::cast<float>(t[4]);
            c.boost = nb::cast<bool>(t[5]);
            c.jump = nb::cast<bool>(t[6]);
            c.handbrake = nb::cast<bool>(t[7]);
        });

    // ========== BallState class ==========
    nb::class_<BallState>(m, "BallState")
        .def(nb::init<>())
        .def("__init__", [](BallState* self,
                           std::optional<Vec> pos,
                           std::optional<Vec> vel,
                           std::optional<Vec> ang_vel,
                           std::optional<RotMat> rot_mat,
                           std::optional<uint32_t> last_hit_car_id) {
            new (self) BallState();
            if (pos) self->pos = *pos;
            if (vel) self->vel = *vel;
            if (ang_vel) self->angVel = *ang_vel;
            if (rot_mat) self->rotMat = *rot_mat;
            if (last_hit_car_id) self->lastHitCarID = *last_hit_car_id;
        }, "pos"_a = std::nullopt, "vel"_a = std::nullopt, "ang_vel"_a = std::nullopt, 
           "rot_mat"_a = std::nullopt, "last_hit_car_id"_a = std::nullopt)
        .def_rw("pos", &BallState::pos)
        .def_rw("vel", &BallState::vel)
        .def_rw("ang_vel", &BallState::angVel)
        .def_rw("rot_mat", &BallState::rotMat)
        .def_rw("last_hit_car_id", &BallState::lastHitCarID);

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
    // Use CarConfig(CarConfig.OCTANE) or CarConfig(rs.OCTANE) for presets
    auto car_config = nb::class_<CarConfig>(m, "CarConfig")
        .def(nb::init<>())  // Default C++ constructor (empty config)
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
        }, "hitbox_type"_a)
        .def_rw("hitbox_size", &CarConfig::hitboxSize)
        .def_rw("hitbox_pos_offset", &CarConfig::hitboxPosOffset)
        .def_rw("front_wheels", &CarConfig::frontWheels)
        .def_rw("back_wheels", &CarConfig::backWheels)
        .def_rw("dodge_deadzone", &CarConfig::dodgeDeadzone);
    
    // Class-level constants: CarConfig.OCTANE, CarConfig.DOMINUS, etc.
    car_config.attr("OCTANE") = 0;
    car_config.attr("DOMINUS") = 1;
    car_config.attr("PLANK") = 2;
    car_config.attr("BREAKOUT") = 3;
    car_config.attr("HYBRID") = 4;
    car_config.attr("MERC") = 5;
    

    // ========== CarState class ==========
    nb::class_<CarState>(m, "CarState")
        .def(nb::init<>())
        .def("__init__", [](CarState* self,
                           std::optional<Vec> pos,
                           std::optional<Vec> vel,
                           std::optional<Vec> ang_vel,
                           std::optional<RotMat> rot_mat,
                           std::optional<float> boost,
                           std::optional<bool> is_on_ground,
                           std::optional<bool> is_demoed) {
            new (self) CarState();
            if (pos) self->pos = *pos;
            if (vel) self->vel = *vel;
            if (ang_vel) self->angVel = *ang_vel;
            if (rot_mat) self->rotMat = *rot_mat;
            if (boost) self->boost = *boost;
            if (is_on_ground) self->isOnGround = *is_on_ground;
            if (is_demoed) self->isDemoed = *is_demoed;
        }, "pos"_a = std::nullopt, "vel"_a = std::nullopt, "ang_vel"_a = std::nullopt,
           "rot_mat"_a = std::nullopt, "boost"_a = std::nullopt, "is_on_ground"_a = std::nullopt,
           "is_demoed"_a = std::nullopt)
        .def_rw("pos", &CarState::pos)
        .def_rw("rot_mat", &CarState::rotMat)
        .def_rw("vel", &CarState::vel)
        .def_rw("ang_vel", &CarState::angVel)
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
        .def_rw("has_jumped", &CarState::hasJumped)
        .def_rw("is_jumping", &CarState::isJumping)
        .def_rw("jump_time", &CarState::jumpTime)
        .def_rw("has_double_jumped", &CarState::hasDoubleJumped)
        .def_rw("air_time_since_jump", &CarState::airTimeSinceJump)
        .def_rw("has_flipped", &CarState::hasFlipped)
        .def_rw("is_flipping", &CarState::isFlipping)
        .def_rw("flip_time", &CarState::flipTime)
        .def_rw("flip_rel_torque", &CarState::flipRelTorque)
        .def_rw("is_auto_flipping", &CarState::isAutoFlipping)
        .def_rw("auto_flip_timer", &CarState::autoFlipTimer)
        .def_rw("auto_flip_torque_scale", &CarState::autoFlipTorqueScale)
        .def_rw("boost", &CarState::boost)
        .def_rw("time_spent_boosting", &CarState::boostingTime)
        .def_rw("is_supersonic", &CarState::isSupersonic)
        .def_rw("supersonic_time", &CarState::supersonicTime)
        .def_rw("handbrake_val", &CarState::handbrakeVal)
        .def_rw("is_demoed", &CarState::isDemoed)
        .def_rw("demo_respawn_timer", &CarState::demoRespawnTimer)
        .def_prop_rw("car_contact_id",
            [](const CarState& s) { return s.carContact.otherCarID; },
            [](CarState& s, uint32_t id) { s.carContact.otherCarID = id; })
        .def_prop_rw("car_contact_cooldown_timer",
            [](const CarState& s) { return s.carContact.cooldownTimer; },
            [](CarState& s, float t) { s.carContact.cooldownTimer = t; })
        .def_prop_rw("has_world_contact",
            [](const CarState& s) { return s.worldContact.hasContact; },
            [](CarState& s, bool v) { s.worldContact.hasContact = v; })
        .def_prop_rw("world_contact_normal",
            [](const CarState& s) { return s.worldContact.contactNormal; },
            [](CarState& s, const Vec& v) { s.worldContact.contactNormal = v; })
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
        .def_prop_ro("team", [](const Car& c) { return c.team; });

    // ========== Arena class (raw, for advanced use) ==========
    nb::class_<Arena>(m, "_Arena")
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
        .def("get_car_from_id", [](Arena* a, uint32_t id, nb::object default_val) -> nb::object {
            Car* car = a->GetCar(id);
            if (car) {
                return nb::cast(car, nb::rv_policy::reference);
            }
            return default_val;
        }, "car_id"_a, "default"_a = nb::none())
        .def("get_boost_pads", [](Arena* a) {
            return a->GetBoostPads();
        }, nb::rv_policy::reference)
        .def("set_mutator_config", &Arena::SetMutatorConfig)
        .def("get_mutator_config", &Arena::GetMutatorConfig, nb::rv_policy::reference)
        .def("reset_to_random_kickoff", &Arena::ResetToRandomKickoff, "seed"_a = -1)
        .def("is_ball_probably_going_in", &Arena::IsBallProbablyGoingIn, 
             "max_time"_a = 2.0f, "extra_margin"_a = 0.0f, "goal_team_out"_a = nullptr)
        .def("is_ball_scored", &Arena::IsBallScored)
        .def_prop_ro("ball", [](Arena* a) { return a->ball; }, nb::rv_policy::reference)
        .def_prop_ro("game_mode", [](const Arena& a) { return a.gameMode; })
        .def_prop_ro("tick_count", [](const Arena& a) { return a.tickCount; })
        .def_prop_ro("tick_rate", &Arena::GetTickRate)
        .def_prop_ro("tick_time", [](const Arena& a) { return a.tickTime; });

    // ========== ArenaWrapper class (with RLGym features) ==========
    nb::class_<ArenaWrapper>(m, "Arena")
        .def(nb::init<GameMode, float>(), "game_mode"_a, "tick_rate"_a = 120.0f)
        .def("step", &ArenaWrapper::step, "ticks_to_simulate"_a = 1)
        .def("clone", [](ArenaWrapper* a, bool copy_callbacks) { return a->clone(copy_callbacks); },
             nb::rv_policy::take_ownership, "copy_callbacks"_a = false)
        .def("add_car", &ArenaWrapper::add_car, "team"_a, "config"_a, nb::rv_policy::reference)
        .def("remove_car", &ArenaWrapper::remove_car)
        .def("get_cars", [](ArenaWrapper* a) {
            std::vector<Car*> cars;
            for (auto* car : a->arena->GetCars()) {
                cars.push_back(car);
            }
            return cars;
        }, nb::rv_policy::reference)
        .def("get_car_from_id", [](ArenaWrapper* a, uint32_t id, nb::object default_val) -> nb::object {
            Car* car = a->arena->GetCar(id);
            if (car) {
                return nb::cast(car, nb::rv_policy::reference);
            }
            return default_val;
        }, "car_id"_a, "default"_a = nb::none())
        .def("get_boost_pads", [](ArenaWrapper* a) {
            return a->arena->GetBoostPads();
        }, nb::rv_policy::reference)
        .def("set_mutator_config", [](ArenaWrapper* a, const MutatorConfig& cfg) {
            a->arena->SetMutatorConfig(cfg);
        })
        .def("get_mutator_config", [](ArenaWrapper* a) -> const MutatorConfig& {
            return a->arena->GetMutatorConfig();
        }, nb::rv_policy::reference)
        .def("reset_to_random_kickoff", &ArenaWrapper::reset_to_random_kickoff, "seed"_a = -1)
        .def("is_ball_probably_going_in", [](ArenaWrapper* a, float maxTime, float extraMargin, Team* goalTeamOut) {
            return a->arena->IsBallProbablyGoingIn(maxTime, extraMargin, goalTeamOut);
        }, "max_time"_a = 2.0f, "extra_margin"_a = 0.0f, "goal_team_out"_a = nullptr)
        .def("is_ball_scored", [](ArenaWrapper* a) { return a->arena->IsBallScored(); })
        // Ball property
        .def_prop_ro("ball", [](ArenaWrapper* a) { return a->arena->ball; }, nb::rv_policy::reference)
        // Game state properties
        .def_prop_ro("game_mode", [](ArenaWrapper* a) { return a->arena->gameMode; })
        .def_prop_ro("tick_count", [](ArenaWrapper* a) { return a->arena->tickCount; })
        .def_prop_ro("tick_rate", [](ArenaWrapper* a) { return a->arena->GetTickRate(); })
        .def_prop_ro("tick_time", [](ArenaWrapper* a) { return a->arena->tickTime; })
        // Score tracking
        .def_prop_ro("blue_score", [](ArenaWrapper* a) { return a->blue_score; })
        .def_prop_ro("orange_score", [](ArenaWrapper* a) { return a->orange_score; })
        // Car stats
        .def("get_car_goals", [](ArenaWrapper* a, uint32_t car_id) {
            auto it = a->car_stats.find(car_id);
            return it != a->car_stats.end() ? it->second.goals : 0;
        }, "car_id"_a)
        .def("get_car_demos", [](ArenaWrapper* a, uint32_t car_id) {
            auto it = a->car_stats.find(car_id);
            return it != a->car_stats.end() ? it->second.demos : 0;
        }, "car_id"_a)
        .def("get_car_boost_pickups", [](ArenaWrapper* a, uint32_t car_id) {
            auto it = a->car_stats.find(car_id);
            return it != a->car_stats.end() ? it->second.boost_pickups : 0;
        }, "car_id"_a)
        // Callbacks
        .def("set_goal_score_callback", [](ArenaWrapper* a, nb::object callback, nb::object data) {
            if (a->arena->gameMode == GameMode::THE_VOID) {
                throw std::runtime_error("Cannot set goal score callback in THE_VOID game mode");
            }
            nb::object prev_cb = a->goal_score_callback ? a->goal_score_callback : nb::none();
            nb::object prev_data = a->goal_score_data ? a->goal_score_data : nb::none();
            a->goal_score_callback = callback;
            a->goal_score_data = data;
            return nb::make_tuple(prev_cb, prev_data);
        }, "callback"_a, "data"_a = nb::none(),
           "Set goal score callback. callback(arena, scoring_team, data) called with kwargs. Returns previous (callback, data).")
        .def("set_car_bump_callback", [](ArenaWrapper* a, nb::object callback, nb::object data) {
            nb::object prev_cb = a->car_bump_callback ? a->car_bump_callback : nb::none();
            nb::object prev_data = a->car_bump_data ? a->car_bump_data : nb::none();
            a->car_bump_callback = callback;
            a->car_bump_data = data;
            return nb::make_tuple(prev_cb, prev_data);
        }, "callback"_a, "data"_a = nb::none(),
           "Set car bump callback. callback(arena, bumper, victim, is_demo, data) called with kwargs. Returns previous (callback, data).")
        .def("set_boost_pickup_callback", [](ArenaWrapper* a, nb::object callback, nb::object data) {
            if (a->arena->gameMode == GameMode::THE_VOID) {
                throw std::runtime_error("Cannot set boost pickup callback in THE_VOID game mode");
            }
            nb::object prev_cb = a->boost_pickup_callback ? a->boost_pickup_callback : nb::none();
            nb::object prev_data = a->boost_pickup_data ? a->boost_pickup_data : nb::none();
            a->boost_pickup_callback = callback;
            a->boost_pickup_data = data;
            return nb::make_tuple(prev_cb, prev_data);
        }, "callback"_a, "data"_a = nb::none(),
           "Set boost pickup callback. callback(arena, car, boost_pad, data) called with kwargs. Returns previous (callback, data).")
        .def("set_ball_touch_callback", [](ArenaWrapper* a, nb::object callback, nb::object data) {
            nb::object prev_cb = a->ball_touch_callback ? a->ball_touch_callback : nb::none();
            nb::object prev_data = a->ball_touch_data ? a->ball_touch_data : nb::none();
            a->ball_touch_callback = callback;
            a->ball_touch_data = data;
            // Only set C++ callback if Python callback is set (avoid overhead)
            if (callback && !callback.is_none()) {
                a->arena->SetBallTouchCallback([](Arena* arena, Car* car, void* userInfo) {
                    auto* self = static_cast<ArenaWrapper*>(userInfo);
                    if (self->ball_touch_callback) {
                        nb::gil_scoped_acquire gil;
                        try {
                            self->ball_touch_callback(
                                "arena"_a = nb::cast(self, nb::rv_policy::reference),
                                "car"_a = nb::cast(car, nb::rv_policy::reference),
                                "data"_a = self->ball_touch_data
                            );
                        } catch (...) {}
                    }
                }, a);
            } else {
                a->arena->SetBallTouchCallback(nullptr, nullptr);
            }
            return nb::make_tuple(prev_cb, prev_data);
        }, "callback"_a, "data"_a = nb::none(),
           "Set ball touch callback. callback(arena, car, data) called with kwargs. Returns previous (callback, data).")
        // ====== EFFICIENT GYM STATE GETTERS ======
        .def("get_ball_state_array", &ArenaWrapper::get_ball_state,
             "Get ball state as numpy array [pos(3), vel(3), ang_vel(3), rot_mat(9)]")
        .def("get_car_state_array", &ArenaWrapper::get_car_state, "car"_a,
             "Get single car state as numpy array [pos(3), vel(3), ang_vel(3), rot_mat(9), boost, on_ground, jumped, double_jumped, flipped, demoed, supersonic, ball_touched]")
        .def("get_cars_state_array", &ArenaWrapper::get_cars_state,
             "Get all cars state as (N, 26) numpy array")
        .def("get_pads_state_array", &ArenaWrapper::get_pads_state,
             "Get boost pad states as numpy array of 0/1 values")
        .def("get_gym_state", &ArenaWrapper::get_gym_state,
             "Get complete gym state as dict with numpy arrays - most efficient for RLGym");

    // ========== Car config presets ==========
    m.attr("CAR_CONFIG_OCTANE") = &CAR_CONFIG_OCTANE;
    m.attr("CAR_CONFIG_DOMINUS") = &CAR_CONFIG_DOMINUS;
    m.attr("CAR_CONFIG_PLANK") = &CAR_CONFIG_PLANK;
    m.attr("CAR_CONFIG_BREAKOUT") = &CAR_CONFIG_BREAKOUT;
    m.attr("CAR_CONFIG_HYBRID") = &CAR_CONFIG_HYBRID;
    m.attr("CAR_CONFIG_MERC") = &CAR_CONFIG_MERC;

    // ========== Hitbox type constants ==========
    // Module-level (backwards compatible)
    m.attr("OCTANE") = 0;
    m.attr("DOMINUS") = 1;
    m.attr("PLANK") = 2;
    m.attr("BREAKOUT") = 3;
    m.attr("HYBRID") = 4;
    m.attr("MERC") = 5;
}
