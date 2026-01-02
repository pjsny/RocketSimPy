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
#include "Sim/Arena/ArenaConfig/ArenaConfig.h"

#include "RLViserSocket.h"

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cstring>
#include <algorithm>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <sstream>

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
    nb::object car_demo_callback;  // Separate demo callback
    nb::object car_demo_data;
    nb::object boost_pickup_callback;
    nb::object boost_pickup_data;
    nb::object ball_touch_callback;
    nb::object ball_touch_data;
    
    // Reusable gym state buffer
    GymState gym_state;
    
    // Track last gym state tick for ball_touched detection
    uint64_t last_gym_state_tick = 0;
    
    // Exception tracking for callbacks (for multi_step)
    // Store exception info to be re-raised after stepping completes
    std::exception_ptr stored_exception;
    std::mutex exception_mutex;
    
    // Helper to store exception and stop simulation
    void store_exception_and_stop() {
        std::lock_guard<std::mutex> lock(exception_mutex);
        if (!stored_exception) {
            stored_exception = std::current_exception();
            arena->Stop();
        }
    }
    
    // Helper to check and rethrow stored exception
    void check_and_rethrow() {
        std::lock_guard<std::mutex> lock(exception_mutex);
        if (stored_exception) {
            std::exception_ptr ex = stored_exception;
            stored_exception = nullptr;
            std::rethrow_exception(ex);
        }
    }
    
    // Clear stored exception
    void clear_exception() {
        std::lock_guard<std::mutex> lock(exception_mutex);
        stored_exception = nullptr;
    }
    
    // Check if has stored exception
    bool has_exception() {
        std::lock_guard<std::mutex> lock(exception_mutex);
        return stored_exception != nullptr;
    }
    
    ArenaWrapper(GameMode mode, float tick_rate, ArenaMemWeightMode mem_weight_mode = ArenaMemWeightMode::HEAVY,
                 std::optional<std::vector<BoostPadConfig>> custom_boost_pads = std::nullopt) 
        : arena(nullptr, [](Arena* a) { if (a) delete a; })
    {
        // Validate tick rate (RL uses 120, but allow reasonable range)
        if (tick_rate < 15.0f || tick_rate > 120.0f) {
            throw std::invalid_argument("tick_rate must be between 15 and 120");
        }
        
        ArenaConfig config{};
        config.memWeightMode = mem_weight_mode;
        
        // Handle custom boost pads
        if (custom_boost_pads && !custom_boost_pads->empty()) {
            config.useCustomBoostPads = true;
            config.customBoostPads = *custom_boost_pads;
        }
        
        arena.reset(Arena::Create(mode, config, tick_rate));
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
                    } catch (...) {
                        self->store_exception_and_stop();
                    }
                }
            }, this);
        }
        
        // Car bump/demo callback
        arena->SetCarBumpCallback([](Arena* a, Car* bumper, Car* victim, bool isDemo, void* userInfo) {
            auto* self = static_cast<ArenaWrapper*>(userInfo);
            
            if (isDemo) {
                self->car_stats[bumper->id].demos++;
                
                // Call separate demo callback if set
                if (self->car_demo_callback) {
                    nb::gil_scoped_acquire gil;
                    try {
                        self->car_demo_callback(
                            "arena"_a = nb::cast(self, nb::rv_policy::reference),
                            "bumper"_a = nb::cast(bumper, nb::rv_policy::reference),
                            "victim"_a = nb::cast(victim, nb::rv_policy::reference),
                            "data"_a = self->car_demo_data
                        );
                    } catch (...) {
                        self->store_exception_and_stop();
                    }
                }
            }
            
            // Call bump callback if set (for all bumps including demos)
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
                } catch (...) {
                    self->store_exception_and_stop();
                }
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
                    } catch (...) {
                        self->store_exception_and_stop();
                    }
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
        // Clear any previous exceptions
        clear_exception();
        
        // Release GIL during physics stepping for better threading performance
        {
            nb::gil_scoped_release release;
            arena->Step(ticks);
        }
        
        // Check and rethrow any exceptions from callbacks
        check_and_rethrow();
    }
    
    void stop() {
        arena->Stop();
    }
    
    // Step without releasing GIL (for use in multi_step where GIL is already released)
    void step_internal(int ticks) {
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
            cloned->car_demo_callback = car_demo_callback;
            cloned->car_demo_data = car_demo_data;
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
                        } catch (...) {
                            self->store_exception_and_stop();
                        }
                    }
                }, cloned);
            }
        }
        return cloned;
    }
    
    // ========================================================================
    // Efficient gym state getters - return numpy arrays with minimal overhead
    // ========================================================================
    
    // Helper: Write ball state to array at offset, optionally inverted
    // Inversion mirrors coordinates for opposing team perspective: (-x, -y, z)
    static void write_ball_state(float* data, const BallState& bs, bool inverted) {
        float sign = inverted ? -1.0f : 1.0f;
        // Position (inverted: -x, -y, z)
        data[0] = sign * bs.pos.x; data[1] = sign * bs.pos.y; data[2] = bs.pos.z;
        // Velocity (inverted: -x, -y, z)
        data[3] = sign * bs.vel.x; data[4] = sign * bs.vel.y; data[5] = bs.vel.z;
        // Angular velocity (inverted: -x, -y, z)
        data[6] = sign * bs.angVel.x; data[7] = sign * bs.angVel.y; data[8] = bs.angVel.z;
        // Rotation matrix (each vector inverted: -x, -y, z)
        data[9] = sign * bs.rotMat.forward.x; data[10] = sign * bs.rotMat.forward.y; data[11] = bs.rotMat.forward.z;
        data[12] = sign * bs.rotMat.right.x; data[13] = sign * bs.rotMat.right.y; data[14] = bs.rotMat.right.z;
        data[15] = sign * bs.rotMat.up.x; data[16] = sign * bs.rotMat.up.y; data[17] = bs.rotMat.up.z;
    }
    
    // Helper: Write car state to array at offset, optionally inverted
    static void write_car_state(float* data, const CarState& cs, bool inverted, bool ball_touched) {
        float sign = inverted ? -1.0f : 1.0f;
        // Position (inverted: -x, -y, z)
        data[0] = sign * cs.pos.x; data[1] = sign * cs.pos.y; data[2] = cs.pos.z;
        // Velocity (inverted: -x, -y, z)
        data[3] = sign * cs.vel.x; data[4] = sign * cs.vel.y; data[5] = cs.vel.z;
        // Angular velocity (inverted: -x, -y, z)
        data[6] = sign * cs.angVel.x; data[7] = sign * cs.angVel.y; data[8] = cs.angVel.z;
        // Rotation matrix (each vector inverted: -x, -y, z)
        data[9] = sign * cs.rotMat.forward.x; data[10] = sign * cs.rotMat.forward.y; data[11] = cs.rotMat.forward.z;
        data[12] = sign * cs.rotMat.right.x; data[13] = sign * cs.rotMat.right.y; data[14] = cs.rotMat.right.z;
        data[15] = sign * cs.rotMat.up.x; data[16] = sign * cs.rotMat.up.y; data[17] = cs.rotMat.up.z;
        // State flags (not inverted)
        data[18] = cs.boost;
        data[19] = cs.isOnGround ? 1.0f : 0.0f;
        data[20] = cs.hasJumped ? 1.0f : 0.0f;
        data[21] = cs.hasDoubleJumped ? 1.0f : 0.0f;
        data[22] = cs.hasFlipped ? 1.0f : 0.0f;
        data[23] = cs.isDemoed ? 1.0f : 0.0f;
        data[24] = cs.isSupersonic ? 1.0f : 0.0f;
        data[25] = ball_touched ? 1.0f : 0.0f;
    }
    
    // Get ball state array
    // If inverted=false: returns shape (18,) with normal view
    // If inverted=true: returns shape (2, 18) with [normal, inverted] views
    nb::ndarray<nb::numpy, float> get_ball_state(bool inverted = false) {
        BallState bs = arena->ball->GetState();
        
        if (!inverted) {
            float* data = new float[18];
            write_ball_state(data, bs, false);
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float>(data, {18}, owner);
        } else {
            float* data = new float[2 * 18];
            write_ball_state(data, bs, false);        // Row 0: normal
            write_ball_state(data + 18, bs, true);    // Row 1: inverted
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float>(data, {2, 18}, owner);
        }
    }
    
    // Get single car state array
    // If inverted=false: returns shape (26,) with normal view
    // If inverted=true: returns shape (2, 26) with [normal, inverted] views
    nb::ndarray<nb::numpy, float> get_car_state(Car* car, bool inverted = false) {
        CarState cs = car->GetState();
        bool ball_touched = cs.ballHitInfo.isValid && 
                           cs.ballHitInfo.tickCountWhenHit >= last_gym_state_tick;
        
        if (!inverted) {
            float* data = new float[26];
            write_car_state(data, cs, false, ball_touched);
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float>(data, {26}, owner);
        } else {
            float* data = new float[2 * 26];
            write_car_state(data, cs, false, ball_touched);       // Row 0: normal
            write_car_state(data + 26, cs, true, ball_touched);   // Row 1: inverted
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float>(data, {2, 26}, owner);
        }
    }
    
    // Get all cars state array
    // If inverted=false: returns shape (N, 26) with normal views
    // If inverted=true: returns shape (N, 2, 26) with [normal, inverted] views per car
    nb::ndarray<nb::numpy, float> get_cars_state(bool inverted = false) {
        const auto& cars = arena->GetCars();
        size_t n = cars.size();
        
        if (!inverted) {
            float* data = new float[n * 26];
            size_t i = 0;
            for (Car* car : cars) {
                CarState cs = car->GetState();
                bool ball_touched = cs.ballHitInfo.isValid && 
                                   cs.ballHitInfo.tickCountWhenHit >= last_gym_state_tick;
                write_car_state(data + i * 26, cs, false, ball_touched);
                i++;
            }
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float>(data, {n, 26}, owner);
        } else {
            float* data = new float[n * 2 * 26];
            size_t i = 0;
            for (Car* car : cars) {
                CarState cs = car->GetState();
                bool ball_touched = cs.ballHitInfo.isValid && 
                                   cs.ballHitInfo.tickCountWhenHit >= last_gym_state_tick;
                float* base = data + i * 2 * 26;
                write_car_state(base, cs, false, ball_touched);       // Row 0: normal
                write_car_state(base + 26, cs, true, ball_touched);   // Row 1: inverted
                i++;
            }
            nb::capsule owner(data, [](void* p) noexcept { delete[] static_cast<float*>(p); });
            return nb::ndarray<nb::numpy, float>(data, {n, 2, 26}, owner);
        }
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
    // If inverted=true, ball and cars arrays include both normal and inverted views
    // Ball shape: (18,) or (2, 18), Cars shape: (N, 26) or (N, 2, 26)
    nb::dict get_gym_state(bool inverted = false) {
        nb::dict result;
        result["ball"] = get_ball_state(inverted);
        result["cars"] = get_cars_state(inverted);
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

    // ========== MemoryWeightMode enum ==========
    nb::enum_<ArenaMemWeightMode>(m, "MemoryWeightMode")
        .value("HEAVY", ArenaMemWeightMode::HEAVY)
        .value("LIGHT", ArenaMemWeightMode::LIGHT);

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
        .def_rw("last_hit_car_id", &BallState::lastHitCarID)
        // Pickle support
        .def("__getstate__", [](const BallState& s) {
            return nb::make_tuple(
                nb::make_tuple(s.pos.x, s.pos.y, s.pos.z),
                nb::make_tuple(s.vel.x, s.vel.y, s.vel.z),
                nb::make_tuple(s.angVel.x, s.angVel.y, s.angVel.z),
                nb::make_tuple(
                    nb::make_tuple(s.rotMat.forward.x, s.rotMat.forward.y, s.rotMat.forward.z),
                    nb::make_tuple(s.rotMat.right.x, s.rotMat.right.y, s.rotMat.right.z),
                    nb::make_tuple(s.rotMat.up.x, s.rotMat.up.y, s.rotMat.up.z)
                ),
                s.lastHitCarID
            );
        })
        .def("__setstate__", [](BallState& s, nb::tuple t) {
            new (&s) BallState();
            auto pos = nb::cast<nb::tuple>(t[0]);
            s.pos = Vec(nb::cast<float>(pos[0]), nb::cast<float>(pos[1]), nb::cast<float>(pos[2]));
            auto vel = nb::cast<nb::tuple>(t[1]);
            s.vel = Vec(nb::cast<float>(vel[0]), nb::cast<float>(vel[1]), nb::cast<float>(vel[2]));
            auto angVel = nb::cast<nb::tuple>(t[2]);
            s.angVel = Vec(nb::cast<float>(angVel[0]), nb::cast<float>(angVel[1]), nb::cast<float>(angVel[2]));
            auto rotMat = nb::cast<nb::tuple>(t[3]);
            auto fwd = nb::cast<nb::tuple>(rotMat[0]);
            auto right = nb::cast<nb::tuple>(rotMat[1]);
            auto up = nb::cast<nb::tuple>(rotMat[2]);
            s.rotMat = RotMat(
                Vec(nb::cast<float>(fwd[0]), nb::cast<float>(fwd[1]), nb::cast<float>(fwd[2])),
                Vec(nb::cast<float>(right[0]), nb::cast<float>(right[1]), nb::cast<float>(right[2])),
                Vec(nb::cast<float>(up[0]), nb::cast<float>(up[1]), nb::cast<float>(up[2]))
            );
            s.lastHitCarID = nb::cast<uint32_t>(t[4]);
        });

    // ========== BoostPadConfig class ==========
    nb::class_<BoostPadConfig>(m, "BoostPadConfig")
        .def(nb::init<>())
        .def("__init__", [](BoostPadConfig* self, std::optional<Vec> pos, std::optional<bool> is_big) {
            new (self) BoostPadConfig();
            if (pos) self->pos = *pos;
            if (is_big) self->isBig = *is_big;
        }, "pos"_a = std::nullopt, "is_big"_a = std::nullopt)
        .def_rw("pos", &BoostPadConfig::pos)
        .def_rw("is_big", &BoostPadConfig::isBig)
        .def("__repr__", [](const BoostPadConfig& c) {
            std::ostringstream ss;
            ss << "BoostPadConfig(pos=Vec(" << c.pos.x << ", " << c.pos.y << ", " << c.pos.z << "), is_big=" << (c.isBig ? "True" : "False") << ")";
            return ss.str();
        });

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
                           std::optional<bool> is_demoed,
                           std::optional<bool> has_jumped,
                           std::optional<bool> has_double_jumped,
                           std::optional<bool> has_flipped,
                           std::optional<bool> is_flipping,
                           std::optional<bool> is_jumping,
                           std::optional<float> jump_time,
                           std::optional<float> flip_time,
                           std::optional<float> air_time_since_jump) {
            new (self) CarState();
            if (pos) self->pos = *pos;
            if (vel) self->vel = *vel;
            if (ang_vel) self->angVel = *ang_vel;
            if (rot_mat) self->rotMat = *rot_mat;
            if (boost) self->boost = *boost;
            if (is_on_ground) self->isOnGround = *is_on_ground;
            if (is_demoed) self->isDemoed = *is_demoed;
            if (has_jumped) self->hasJumped = *has_jumped;
            if (has_double_jumped) self->hasDoubleJumped = *has_double_jumped;
            if (has_flipped) self->hasFlipped = *has_flipped;
            if (is_flipping) self->isFlipping = *is_flipping;
            if (is_jumping) self->isJumping = *is_jumping;
            if (jump_time) self->jumpTime = *jump_time;
            if (flip_time) self->flipTime = *flip_time;
            if (air_time_since_jump) self->airTimeSinceJump = *air_time_since_jump;
        }, "pos"_a = std::nullopt, "vel"_a = std::nullopt, "ang_vel"_a = std::nullopt,
           "rot_mat"_a = std::nullopt, "boost"_a = std::nullopt, "is_on_ground"_a = std::nullopt,
           "is_demoed"_a = std::nullopt, "has_jumped"_a = std::nullopt, "has_double_jumped"_a = std::nullopt,
           "has_flipped"_a = std::nullopt, "is_flipping"_a = std::nullopt, "is_jumping"_a = std::nullopt,
           "jump_time"_a = std::nullopt, "flip_time"_a = std::nullopt, "air_time_since_jump"_a = std::nullopt)
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
        .def_rw("last_controls", &CarState::lastControls)
        // Pickle support
        .def("__getstate__", [](const CarState& s) {
            return nb::make_tuple(
                // Position and orientation
                nb::make_tuple(s.pos.x, s.pos.y, s.pos.z),
                nb::make_tuple(s.vel.x, s.vel.y, s.vel.z),
                nb::make_tuple(s.angVel.x, s.angVel.y, s.angVel.z),
                nb::make_tuple(
                    nb::make_tuple(s.rotMat.forward.x, s.rotMat.forward.y, s.rotMat.forward.z),
                    nb::make_tuple(s.rotMat.right.x, s.rotMat.right.y, s.rotMat.right.z),
                    nb::make_tuple(s.rotMat.up.x, s.rotMat.up.y, s.rotMat.up.z)
                ),
                // State flags
                s.isOnGround, s.hasJumped, s.isJumping, s.hasDoubleJumped, s.hasFlipped, s.isFlipping,
                // Timing
                s.jumpTime, s.flipTime, s.airTimeSinceJump,
                // Flip
                nb::make_tuple(s.flipRelTorque.x, s.flipRelTorque.y, s.flipRelTorque.z),
                s.isAutoFlipping, s.autoFlipTimer, s.autoFlipTorqueScale,
                // Boost
                s.boost, s.boostingTime,
                // Supersonic
                s.isSupersonic, s.supersonicTime,
                // Other
                s.handbrakeVal, s.isDemoed, s.demoRespawnTimer,
                // Contacts
                s.carContact.otherCarID, s.carContact.cooldownTimer,
                s.worldContact.hasContact,
                nb::make_tuple(s.worldContact.contactNormal.x, s.worldContact.contactNormal.y, s.worldContact.contactNormal.z)
            );
        })
        .def("__setstate__", [](CarState& s, nb::tuple t) {
            new (&s) CarState();
            auto pos = nb::cast<nb::tuple>(t[0]);
            s.pos = Vec(nb::cast<float>(pos[0]), nb::cast<float>(pos[1]), nb::cast<float>(pos[2]));
            auto vel = nb::cast<nb::tuple>(t[1]);
            s.vel = Vec(nb::cast<float>(vel[0]), nb::cast<float>(vel[1]), nb::cast<float>(vel[2]));
            auto angVel = nb::cast<nb::tuple>(t[2]);
            s.angVel = Vec(nb::cast<float>(angVel[0]), nb::cast<float>(angVel[1]), nb::cast<float>(angVel[2]));
            auto rotMat = nb::cast<nb::tuple>(t[3]);
            auto fwd = nb::cast<nb::tuple>(rotMat[0]);
            auto right = nb::cast<nb::tuple>(rotMat[1]);
            auto up = nb::cast<nb::tuple>(rotMat[2]);
            s.rotMat = RotMat(
                Vec(nb::cast<float>(fwd[0]), nb::cast<float>(fwd[1]), nb::cast<float>(fwd[2])),
                Vec(nb::cast<float>(right[0]), nb::cast<float>(right[1]), nb::cast<float>(right[2])),
                Vec(nb::cast<float>(up[0]), nb::cast<float>(up[1]), nb::cast<float>(up[2]))
            );
            s.isOnGround = nb::cast<bool>(t[4]);
            s.hasJumped = nb::cast<bool>(t[5]);
            s.isJumping = nb::cast<bool>(t[6]);
            s.hasDoubleJumped = nb::cast<bool>(t[7]);
            s.hasFlipped = nb::cast<bool>(t[8]);
            s.isFlipping = nb::cast<bool>(t[9]);
            s.jumpTime = nb::cast<float>(t[10]);
            s.flipTime = nb::cast<float>(t[11]);
            s.airTimeSinceJump = nb::cast<float>(t[12]);
            auto flipTorque = nb::cast<nb::tuple>(t[13]);
            s.flipRelTorque = Vec(nb::cast<float>(flipTorque[0]), nb::cast<float>(flipTorque[1]), nb::cast<float>(flipTorque[2]));
            s.isAutoFlipping = nb::cast<bool>(t[14]);
            s.autoFlipTimer = nb::cast<float>(t[15]);
            s.autoFlipTorqueScale = nb::cast<float>(t[16]);
            s.boost = nb::cast<float>(t[17]);
            s.boostingTime = nb::cast<float>(t[18]);
            s.isSupersonic = nb::cast<bool>(t[19]);
            s.supersonicTime = nb::cast<float>(t[20]);
            s.handbrakeVal = nb::cast<float>(t[21]);
            s.isDemoed = nb::cast<bool>(t[22]);
            s.demoRespawnTimer = nb::cast<float>(t[23]);
            s.carContact.otherCarID = nb::cast<uint32_t>(t[24]);
            s.carContact.cooldownTimer = nb::cast<float>(t[25]);
            s.worldContact.hasContact = nb::cast<bool>(t[26]);
            auto contactNormal = nb::cast<nb::tuple>(t[27]);
            s.worldContact.contactNormal = Vec(nb::cast<float>(contactNormal[0]), nb::cast<float>(contactNormal[1]), nb::cast<float>(contactNormal[2]));
        });

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
        .def_rw("enable_team_demos", &MutatorConfig::enableTeamDemos)
        .def_rw("enable_car_car_collision", &MutatorConfig::enableCarCarCollision)
        .def_rw("enable_car_ball_collision", &MutatorConfig::enableCarBallCollision);

    // ========== Ball class ==========
    nb::class_<Ball>(m, "Ball")
        .def("get_state", &Ball::GetState)
        .def("set_state", &Ball::SetState)
        .def("get_radius", &Ball::GetRadius)
        .def("get_rot", [](Ball* ball) {
            auto rot = ball->_rigidBody.getOrientation();
            return nb::make_tuple(rot.getX(), rot.getY(), rot.getZ(), rot.getW());
        }, "Get ball rotation as quaternion (x, y, z, w)");

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
        .def(nb::new_([](GameMode gameMode, float tickRate, ArenaMemWeightMode memWeightMode) {
            ArenaConfig config{};
            config.memWeightMode = memWeightMode;
            return Arena::Create(gameMode, config, tickRate);
        }), "game_mode"_a, "tick_rate"_a = 120.0f, "mem_weight_mode"_a = ArenaMemWeightMode::HEAVY)
        .def("step", &Arena::Step, "ticks_to_simulate"_a = 1)
        .def("stop", &Arena::Stop)
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
        .def("set_car_car_collision", &Arena::SetCarCarCollision, "enable"_a = true)
        .def("set_car_ball_collision", &Arena::SetCarBallCollision, "enable"_a = true)
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
        .def(nb::init<GameMode, float, ArenaMemWeightMode, std::optional<std::vector<BoostPadConfig>>>(), 
             "game_mode"_a, "tick_rate"_a = 120.0f, "mem_weight_mode"_a = ArenaMemWeightMode::HEAVY,
             "custom_boost_pads"_a = std::nullopt,
             R"(Create a new Arena.

Args:
    game_mode: The game mode (SOCCAR, HOOPS, etc.)
    tick_rate: Physics tick rate in Hz (default: 120)
    mem_weight_mode: Memory optimization mode (default: HEAVY)
    custom_boost_pads: Optional list of BoostPadConfig for custom boost pad layouts.
                       If provided, replaces the default boost pads for the game mode.)")
        .def("step", &ArenaWrapper::step, "ticks_to_simulate"_a = 1)
        .def("stop", &ArenaWrapper::stop)
        .def("clone", [](ArenaWrapper* a, bool copy_callbacks) { return a->clone(copy_callbacks); },
             nb::rv_policy::take_ownership, "copy_callbacks"_a = false)
        .def("add_car", &ArenaWrapper::add_car, "team"_a, "config"_a, nb::rv_policy::reference)
        .def("remove_car", [](ArenaWrapper* a, nb::object car_or_id) {
            Car* car = nullptr;
            if (nb::isinstance<Car>(car_or_id)) {
                car = nb::cast<Car*>(car_or_id);
            } else {
                // Assume it's an ID
                uint32_t id = nb::cast<uint32_t>(car_or_id);
                car = a->arena->GetCar(id);
                if (!car) {
                    throw std::invalid_argument("No car with id " + std::to_string(id));
                }
            }
            a->car_stats.erase(car->id);
            a->arena->RemoveCar(car);
        }, "car_or_id"_a, "Remove a car by Car object or car id")
        .def("get_cars", [](ArenaWrapper* a) {
            std::vector<Car*> cars;
            for (auto* car : a->arena->GetCars()) {
                cars.push_back(car);
            }
            // Sort by id for consistent order
            std::sort(cars.begin(), cars.end(), [](Car* a, Car* b) { return a->id < b->id; });
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
        .def("set_car_car_collision", [](ArenaWrapper* a, bool enable) {
            a->arena->SetCarCarCollision(enable);
        }, "enable"_a = true)
        .def("set_car_ball_collision", [](ArenaWrapper* a, bool enable) {
            a->arena->SetCarBallCollision(enable);
        }, "enable"_a = true)
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
        .def("set_car_demo_callback", [](ArenaWrapper* a, nb::object callback, nb::object data) {
            nb::object prev_cb = a->car_demo_callback ? a->car_demo_callback : nb::none();
            nb::object prev_data = a->car_demo_data ? a->car_demo_data : nb::none();
            a->car_demo_callback = callback;
            a->car_demo_data = data;
            return nb::make_tuple(prev_cb, prev_data);
        }, "callback"_a, "data"_a = nb::none(),
           "Set car demo callback. callback(arena, bumper, victim, data) called with kwargs. Returns previous (callback, data).")
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
                        } catch (...) {
                            self->store_exception_and_stop();
                        }
                    }
                }, a);
            } else {
                a->arena->SetBallTouchCallback(nullptr, nullptr);
            }
            return nb::make_tuple(prev_cb, prev_data);
        }, "callback"_a, "data"_a = nb::none(),
           "Set ball touch callback. callback(arena, car, data) called with kwargs. Returns previous (callback, data).")
        // ====== EFFICIENT GYM STATE GETTERS ======
        .def("get_ball_state_array", &ArenaWrapper::get_ball_state, "inverted"_a = false,
             R"(Get ball state as numpy array.
Args:
    inverted: If False, returns shape (18,) [pos(3), vel(3), ang_vel(3), rot_mat(9)]
              If True, returns shape (2, 18) with [normal, inverted] views for both team perspectives)")
        .def("get_car_state_array", &ArenaWrapper::get_car_state, "car"_a, "inverted"_a = false,
             R"(Get single car state as numpy array.
Args:
    car: The car to get state for
    inverted: If False, returns shape (26,) with normal view
              If True, returns shape (2, 26) with [normal, inverted] views)")
        .def("get_cars_state_array", &ArenaWrapper::get_cars_state, "inverted"_a = false,
             R"(Get all cars state as numpy array.
Args:
    inverted: If False, returns shape (N, 26) with normal views
              If True, returns shape (N, 2, 26) with [normal, inverted] views per car)")
        .def("get_pads_state_array", &ArenaWrapper::get_pads_state,
             "Get boost pad states as numpy array of 0/1 values")
        .def("get_gym_state", &ArenaWrapper::get_gym_state, "inverted"_a = false,
             R"(Get complete gym state as dict with numpy arrays.
Args:
    inverted: If True, ball and cars arrays include both normal and inverted views.
              Inverted view mirrors coordinates for opposing team: (-x, -y, z).
              Ball shape: (18,) or (2, 18), Cars shape: (N, 26) or (N, 2, 26))")
        // ====== RLVISER INTEGRATION ======
        .def("render", [](ArenaWrapper* a) {
            return RLViser::get_socket().send_arena_state(a->arena.get());
        }, "Send arena state to RLViser for rendering (uses global socket)")
        .def("get_game_state", [](ArenaWrapper* a) {
            return RLViser::GameState::from_arena(a->arena.get());
        }, "Get the current game state as an RLViser GameState object")
        // ====== MULTI-STEP (PARALLEL SIMULATION) ======
        .def_static("multi_step", [](nb::list arenas_list, int ticks) {
            // Convert list to vector and validate
            std::vector<ArenaWrapper*> arenas;
            std::unordered_set<ArenaWrapper*> seen;
            arenas.reserve(nb::len(arenas_list));
            
            for (size_t i = 0; i < nb::len(arenas_list); ++i) {
                nb::object item = arenas_list[i];
                if (!nb::isinstance<ArenaWrapper>(item)) {
                    throw std::runtime_error("Unexpected type in arenas list - expected Arena objects");
                }
                ArenaWrapper* arena = nb::cast<ArenaWrapper*>(item);
                if (seen.count(arena)) {
                    throw std::runtime_error("Duplicate arena detected in multi_step");
                }
                seen.insert(arena);
                arenas.push_back(arena);
            }
            
            if (arenas.empty()) {
                return;
            }
            
            // Clear exceptions before stepping
            for (auto* arena : arenas) {
                arena->clear_exception();
            }
            
            // For small numbers of arenas or single tick, just step sequentially
            // Thread overhead isn't worth it for small workloads
            const size_t PARALLEL_THRESHOLD = 4;
            
            if (arenas.size() < PARALLEL_THRESHOLD) {
                // Sequential stepping - still release GIL for better Python thread performance
                nb::gil_scoped_release release;
                for (auto* arena : arenas) {
                    arena->step_internal(ticks);
                }
            } else {
                // Parallel stepping with thread pool
                // Use hardware_concurrency as hint, cap at reasonable value
                const unsigned int max_threads = std::min(
                    static_cast<unsigned int>(arenas.size()),
                    std::max(1u, std::thread::hardware_concurrency())
                );
                
                // Release GIL before spawning threads
                nb::gil_scoped_release release;
                
                // Use futures for parallel execution
                std::vector<std::future<void>> futures;
                futures.reserve(arenas.size());
                
                // Simple work distribution - each arena gets its own task
                // Could use work stealing for better load balancing in future
                for (auto* arena : arenas) {
                    futures.emplace_back(std::async(std::launch::async, [arena, ticks]() {
                        arena->step_internal(ticks);
                    }));
                }
                
                // Wait for all to complete
                for (auto& f : futures) {
                    f.get();
                }
            }
            
            // Re-acquire GIL (automatic when release goes out of scope)
            // Check for and rethrow any exceptions from callbacks
            for (auto* arena : arenas) {
                if (arena->has_exception()) {
                    // Rethrow first exception found
                    arena->check_and_rethrow();
                }
            }
        }, "arenas"_a, "ticks"_a = 1,
           R"(Step multiple arenas in parallel.

This method releases the GIL and steps all provided arenas concurrently
using multiple threads for improved performance.

Args:
    arenas: List of Arena objects to step
    ticks: Number of ticks to simulate (default: 1)

Raises:
    RuntimeError: If duplicate arenas are detected or non-Arena objects in list
    Exception: Re-raises any exception from callbacks (simulation stops on exception)

Note:
    - Each arena must be unique (no duplicates)
    - If a callback raises an exception, the arena stops and the exception is re-raised
    - For best performance with many arenas, use MemoryWeightMode.LIGHT
)");

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

    // ========================================================================
    // RLViser Module - UDP communication with RLViser visualizer
    // ========================================================================
    auto rlviser = m.def_submodule("rlviser", "RLViser UDP communication for visualization");

    // Port constants
    rlviser.attr("RLVISER_PORT") = RLViser::RLVISER_PORT;
    rlviser.attr("ROCKETSIM_PORT") = RLViser::ROCKETSIM_PORT;

    // UdpPacketType enum
    nb::enum_<RLViser::UdpPacketType>(rlviser, "UdpPacketType")
        .value("QUIT", RLViser::UdpPacketType::Quit)
        .value("GAME_STATE", RLViser::UdpPacketType::GameState)
        .value("CONNECTION", RLViser::UdpPacketType::Connection)
        .value("PAUSED", RLViser::UdpPacketType::Paused)
        .value("SPEED", RLViser::UdpPacketType::Speed)
        .value("RENDER", RLViser::UdpPacketType::Render);

    // BoostPadInfo class
    nb::class_<RLViser::BoostPadInfo>(rlviser, "BoostPadInfo")
        .def(nb::init<>())
        .def_rw("is_active", &RLViser::BoostPadInfo::isActive)
        .def_rw("cooldown", &RLViser::BoostPadInfo::cooldown)
        .def_rw("pos", &RLViser::BoostPadInfo::pos)
        .def_rw("is_big", &RLViser::BoostPadInfo::isBig);

    // CarInfo class
    nb::class_<RLViser::CarInfo>(rlviser, "CarInfo")
        .def(nb::init<>())
        .def_rw("id", &RLViser::CarInfo::id)
        .def_rw("team", &RLViser::CarInfo::team)
        .def_rw("state", &RLViser::CarInfo::state)
        .def_rw("config", &RLViser::CarInfo::config);

    // BallStateInfo class
    nb::class_<RLViser::BallStateInfo>(rlviser, "BallStateInfo")
        .def(nb::init<>())
        .def_rw("state", &RLViser::BallStateInfo::state);

    // GameState class
    nb::class_<RLViser::GameState>(rlviser, "GameState")
        .def(nb::init<>())
        .def_rw("tick_count", &RLViser::GameState::tickCount)
        .def_rw("tick_rate", &RLViser::GameState::tickRate)
        .def_rw("game_mode", &RLViser::GameState::gameMode)
        .def_rw("pads", &RLViser::GameState::pads)
        .def_rw("cars", &RLViser::GameState::cars)
        .def_rw("ball", &RLViser::GameState::ball)
        .def("to_bytes", [](const RLViser::GameState& self) {
            auto bytes = self.to_bytes();
            return nb::bytes(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        })
        .def_static("from_bytes", [](nb::bytes data) {
            return RLViser::GameState::from_bytes(
                reinterpret_cast<const uint8_t*>(data.c_str()), 
                data.size()
            );
        })
        .def_static("from_arena", [](ArenaWrapper* arena) {
            return RLViser::GameState::from_arena(arena->arena.get());
        }, "arena"_a)
        .def_static("from_raw_arena", [](Arena* arena) {
            return RLViser::GameState::from_arena(arena);
        }, "arena"_a);

    // ReturnMessage class
    nb::class_<RLViser::ReturnMessage>(rlviser, "ReturnMessage")
        .def(nb::init<>())
        .def_prop_ro("game_state", [](const RLViser::ReturnMessage& self) -> nb::object {
            if (self.gameState) return nb::cast(*self.gameState);
            return nb::none();
        })
        .def_prop_ro("speed", [](const RLViser::ReturnMessage& self) -> nb::object {
            if (self.speed) return nb::cast(*self.speed);
            return nb::none();
        })
        .def_prop_ro("paused", [](const RLViser::ReturnMessage& self) -> nb::object {
            if (self.paused) return nb::cast(*self.paused);
            return nb::none();
        });

    // RLViserSocket class
    nb::class_<RLViser::RLViserSocket>(rlviser, "Socket")
        .def(nb::init<>())
        .def("init", &RLViser::RLViserSocket::init,
             "Initialize the UDP socket (binds to port 34254)")
        .def("connect", &RLViser::RLViserSocket::connect,
             "Connect to RLViser (sends connection packet)")
        .def("close", &RLViser::RLViserSocket::close,
             "Close the socket and send quit packet")
        .def("is_connected", &RLViser::RLViserSocket::is_connected)
        .def("send_game_state", &RLViser::RLViserSocket::send_game_state, "state"_a,
             "Send a GameState to RLViser")
        .def("send_arena_state", [](RLViser::RLViserSocket& self, ArenaWrapper* arena) {
            return self.send_arena_state(arena->arena.get());
        }, "arena"_a, "Send current arena state to RLViser")
        .def("send_raw_arena_state", &RLViser::RLViserSocket::send_arena_state, "arena"_a,
             "Send current arena state to RLViser (raw Arena)")
        .def("send_game_speed", &RLViser::RLViserSocket::send_game_speed, "speed"_a,
             "Report game speed to RLViser (1.0 = normal)")
        .def("send_paused", &RLViser::RLViserSocket::send_paused, "paused"_a,
             "Report pause state to RLViser")
        .def("receive_messages", &RLViser::RLViserSocket::receive_messages,
             "Poll for messages from RLViser (non-blocking)")
        .def("is_paused", &RLViser::RLViserSocket::is_paused,
             "Get current pause state")
        .def("get_game_speed", &RLViser::RLViserSocket::get_game_speed,
             "Get current game speed");

    // Convenience functions using global socket
    rlviser.def("init", []() { return RLViser::get_socket().init(); },
        "Initialize the global RLViser socket");
    
    rlviser.def("connect", []() { return RLViser::get_socket().connect(); },
        "Connect the global socket to RLViser");
    
    rlviser.def("close", []() { RLViser::get_socket().close(); },
        "Close the global RLViser socket");
    
    rlviser.def("is_connected", []() { return RLViser::get_socket().is_connected(); },
        "Check if global socket is connected");
    
    rlviser.def("render", [](ArenaWrapper* arena) {
        return RLViser::get_socket().send_arena_state(arena->arena.get());
    }, "arena"_a, "Send arena state to RLViser for rendering");
    
    rlviser.def("render_raw", [](Arena* arena) {
        return RLViser::get_socket().send_arena_state(arena);
    }, "arena"_a, "Send raw Arena state to RLViser for rendering");
    
    rlviser.def("set_game_speed", [](float speed) {
        return RLViser::get_socket().send_game_speed(speed);
    }, "speed"_a, "Set game speed (1.0 = normal)");
    
    rlviser.def("set_paused", [](bool paused) {
        return RLViser::get_socket().send_paused(paused);
    }, "paused"_a, "Set pause state");
    
    rlviser.def("get_state_set", []() {
        return RLViser::get_socket().receive_messages();
    }, "Poll for state changes from RLViser");
    
    rlviser.def("is_paused", []() {
        return RLViser::get_socket().is_paused();
    }, "Get current pause state");
    
    rlviser.def("get_game_speed", []() {
        return RLViser::get_socket().get_game_speed();
    }, "Get current game speed");
}
