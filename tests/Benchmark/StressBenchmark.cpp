#include "StressBenchmark.h"
#include "Sim/Arena/Arena.h"
#include "Sim/Car/Car.h"
#include "Sim/Ball/Ball.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <cmath>

namespace StressBenchmark {

RocketSim::CarControls CalcBotControls(
    const RocketSim::CarState& car_state,
    const RocketSim::BallState& ball_state,
    FastRNG& rng
) {
    // Ball delta from car position
    RocketSim::Vec ball_delta = ball_state.pos - car_state.pos;
    
    // Calculate minimum reach time based on distance and max car speed
    float ball_delta_len = ball_delta.Length();
    float min_reach_time = ball_delta_len / RocketSim::RLConst::CAR_MAX_SPEED;
    
    // Extrapolate ball position (zero out Z velocity for ground prediction)
    RocketSim::Vec extrap_ball_pos = ball_state.pos + RocketSim::Vec(
        ball_state.vel.x * min_reach_time,
        ball_state.vel.y * min_reach_time,
        0.0f  // Zero out Z velocity
    );
    
    // Calculate extrapolated ball delta
    RocketSim::Vec extrap_ball_delta = extrap_ball_pos - car_state.pos;
    
    // Calculate alignment with car's forward and right directions
    float ball_forward_align = extrap_ball_delta.Dot(car_state.rotMat.forward);
    float ball_right_align = extrap_ball_delta.Dot(car_state.rotMat.right);
    
    // Initialize controls
    RocketSim::CarControls controls;
    controls.throttle = 1.0f;
    controls.steer = std::clamp(ball_right_align * 80.0f, -1.0f, 1.0f);
    
    // Driving control
    {
        // Handbrake when ball is behind, we're turning, and somewhat grounded
        if ((ball_forward_align < 0.0f) &&               // Ball is somewhat behind
            (std::abs(car_state.angVel.z) >= 1.0f) &&    // We are turning
            (car_state.pos.z < 300.0f) &&                // We are somewhat grounded
            rng.rand_chance(0.8f))
        {
            controls.handbrake = true;
        }
        
        // Brake proportionally when ball is behind
        if (ball_forward_align < -0.4f) {
            controls.throttle = ball_forward_align;
        }
        
        // Boost when aligned with ball
        if (ball_forward_align < 0.3f) {
            controls.boost = true;
        }
    }
    
    // Jump/air control
    {
        controls.yaw = rng.rand_axis_val();
        controls.pitch = rng.rand_axis_val();
        controls.roll = rng.rand_axis_val() * rng.rand();
        
        if (car_state.isOnGround) {
            controls.jump = rng.rand_chance(0.04f);
        } else {
            if (car_state.isJumping) {
                // Keep holding jump sometimes
                controls.jump = rng.rand_chance(0.5f);
            } else {
                // Flip chance when in air
                controls.jump = rng.rand_chance(0.1f);
            }
            
            // Align flip direction towards ball
            if (!car_state.isJumping && car_state.HasFlipOrJump() && controls.jump) {
                if (rng.rand_chance(0.5f)) {
                    // Align direction towards ball
                    float align_frac = std::sqrt(rng.rand());
                    controls.pitch *= 1.0f - align_frac;
                    controls.yaw *= 1.0f - align_frac;
                    controls.pitch += -ball_forward_align * align_frac;
                    controls.yaw += ball_right_align * align_frac;
                } else if (rng.rand_chance(0.2f)) {
                    // Double-jump (no directional input)
                    controls.yaw = 0.0f;
                    controls.pitch = 0.0f;
                    controls.roll = 0.0f;
                }
            }
        }
    }
    
    // Add randomization to all controls
    {
        float pow3_rand1 = rng.rand();
        pow3_rand1 = pow3_rand1 * pow3_rand1 * pow3_rand1;
        float pow3_rand2 = rng.rand();
        pow3_rand2 = pow3_rand2 * pow3_rand2 * pow3_rand2;
        
        controls.throttle += rng.rand_axis_val() * pow3_rand1;
        controls.steer += rng.rand_axis_val() * pow3_rand2;
        controls.yaw += rng.rand_axis_val() * rng.rand();
        controls.pitch += rng.rand_axis_val() * rng.rand();
        controls.roll += rng.rand_axis_val() * rng.rand();
        
        if (rng.rand_chance(0.2f)) {
            controls.jump = !controls.jump;
        }
        
        if (rng.rand_chance(0.2f)) {
            controls.boost = !controls.boost;
        }
        
        if (rng.rand_chance(0.2f)) {
            controls.handbrake = !controls.handbrake;
        }
    }
    
    // Clamp controls to valid ranges
    controls.ClampFix();
    
    return controls;
}

BenchmarkResult RunBenchmark(
    const std::string& collision_meshes_path,
    uint32_t seed
) {
    BenchmarkResult result;
    result.ticks_simulated = TOTAL_TICKS;
    result.total_ball_touches = 0;
    
    // Initialize RocketSim with collision meshes
    std::filesystem::path meshes_path = collision_meshes_path;
    if (!std::filesystem::exists(meshes_path)) {
        meshes_path = std::filesystem::current_path() / collision_meshes_path;
        if (!std::filesystem::exists(meshes_path)) {
            throw std::runtime_error("Collision meshes path not found: " + collision_meshes_path);
        }
    }
    
    RocketSim::Init(meshes_path, true);  // silent = true
    
    // Create SOCCAR arena at 120Hz
    RocketSim::ArenaConfig config;
    RocketSim::Arena* arena = RocketSim::Arena::Create(RocketSim::GameMode::SOCCAR, config, 120.0f);
    
    // Initialize RNG (use seed 0 if not specified, matching Rust)
    FastRNG rng(seed);
    
    // Add 6 cars (3 per team), alternating teams
    std::vector<RocketSim::Car*> cars;
    for (int i = 0; i < NUM_CARS; i++) {
        RocketSim::Team team = (i % 2 == 0) ? RocketSim::Team::BLUE : RocketSim::Team::ORANGE;
        cars.push_back(arena->AddCar(team, RocketSim::CAR_CONFIG_OCTANE));
    }
    
    // Track ball hits per car for touch detection
    std::vector<uint64_t> last_ball_hit_tick(NUM_CARS, 0);
    
    // Start timing
    ProfilerUtils::Timer timer;
    timer.Start();
    
    // Run episodes
    for (int episode = 0; episode < NUM_EPISODES; episode++) {
        // Set up new episode - reset to kickoff
        arena->ResetToRandomKickoff(static_cast<int>(rng.rand() * 1000000));
        
        // Accelerate the ball randomly
        RocketSim::BallState ball_state = arena->ball->GetState();
        ball_state.vel.x += rng.rand_axis_val() * VEL_ADD_MAG;
        ball_state.vel.y += rng.rand_axis_val() * VEL_ADD_MAG;
        ball_state.vel.z += rng.rand_axis_val() * VEL_ADD_MAG;
        arena->ball->SetState(ball_state);
        
        // Run ticks for this episode
        for (int tick = 0; tick < NUM_EPISODE_TICKS; tick++) {
            RocketSim::BallState current_ball_state = arena->ball->GetState();
            
            for (size_t car_idx = 0; car_idx < cars.size(); car_idx++) {
                RocketSim::Car* car = cars[car_idx];
                RocketSim::CarState car_state = car->GetState();
                
                // Check for ball touch (tick_count == ball_hit_tick + 1)
                if (car_state.ballHitInfo.isValid) {
                    uint64_t hit_tick = car_state.ballHitInfo.tickCountWhenHit;
                    if (arena->tickCount == hit_tick + 1 && hit_tick != last_ball_hit_tick[car_idx]) {
                        result.total_ball_touches++;
                        last_ball_hit_tick[car_idx] = hit_tick;
                    }
                }
                
                // Update controls with UPDATE_CHANCE probability
                if (rng.rand_chance(UPDATE_CHANCE)) {
                    car->controls = CalcBotControls(car_state, current_ball_state, rng);
                }
            }
            
            arena->Step(1);
        }
    }
    
    timer.Stop();
    
    result.elapsed_seconds = timer.GetElapsedSeconds();
    result.ticks_per_second = TOTAL_TICKS / result.elapsed_seconds;
    
    // Cleanup
    delete arena;
    
    return result;
}

void PrintResults(const BenchmarkResult& result) {
    // Print system info first
    ProfilerUtils::PrintSystemInfo();
    
    std::cout << "\n";
    std::cout << "=================================\n";
    std::cout << "Stress Benchmark Results\n";
    std::cout << "=================================\n";
    std::cout << "Configuration:\n";
    std::cout << "  Cars: " << NUM_CARS << " (3v3)\n";
    std::cout << "  Episodes: " << NUM_EPISODES << "\n";
    std::cout << "  Ticks per episode: " << ProfilerUtils::FormatNumber(NUM_EPISODE_TICKS) << "\n";
    std::cout << "  Total ticks: " << ProfilerUtils::FormatNumber(TOTAL_TICKS) << "\n";
    std::cout << "  Control update chance: " << (UPDATE_CHANCE * 100) << "% per tick\n";
    std::cout << "\n";
    std::cout << "Results:\n";
    std::cout << "  Elapsed: " << std::fixed << std::setprecision(3) 
              << result.elapsed_seconds << " seconds\n";
    std::cout << "  TPS: " << std::fixed << std::setprecision(0) 
              << result.ticks_per_second << "\n";
    std::cout << "  Ball hits: " << result.total_ball_touches << "\n";
    std::cout << "=================================\n";
}

} // namespace StressBenchmark

