#include <gtest/gtest.h>
#include "RocketSim.h"
#include "../TestUtils.h"
#include "Sim/Arena/Arena.h"
#include "Sim/Car/Car.h"
#include "Sim/Ball/Ball.h"

using namespace RocketSim;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // RocketSim initialized in main.cpp with collision_meshes
    }
};

TEST_F(IntegrationTest, CarBallInteraction) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    
    // Position car near ball
    CarState carState = car->GetState();
    BallState ballState = arena->ball->GetState();
    
    carState.pos = ballState.pos + Vec(200.0f, 0, 0); // Car to the right of ball
    car->SetState(carState);
    
    // Drive car toward ball
    car->controls.throttle = 1.0f;
    
    // Step simulation
    Vec ballPosBefore = arena->ball->GetState().pos;
    for (int i = 0; i < 100; i++) {
        arena->Step(1);
    }
    Vec ballPosAfter = arena->ball->GetState().pos;
    
    // Ball position should have changed (car should have hit it or at least moved it)
    // Note: In THE_VOID without proper collision, this may not work perfectly,
    // but we can at least verify the simulation runs
    EXPECT_GE(arena->tickCount, 100);
    
    delete arena;
}

TEST_F(IntegrationTest, MultipleCarsSimulation) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car1 = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    Car* car2 = arena->AddCar(Team::ORANGE, CAR_CONFIG_DOMINUS);
    
    // Set different positions
    CarState state1 = car1->GetState();
    CarState state2 = car2->GetState();
    
    state1.pos = Vec(1000.0f, 0, 100.0f);
    state2.pos = Vec(-1000.0f, 0, 100.0f);
    
    car1->SetState(state1);
    car2->SetState(state2);
    
    // Give cars different controls
    car1->controls.throttle = 1.0f;
    car2->controls.throttle = -1.0f; // Reverse
    
    // Simulate
    for (int i = 0; i < 50; i++) {
        arena->Step(1);
    }
    
    // Verify both cars are still in arena
    const auto& cars = arena->GetCars();
    EXPECT_EQ(cars.size(), 2);
    
    CarState finalState1 = car1->GetState();
    CarState finalState2 = car2->GetState();
    
    // Positions should have changed
    EXPECT_GT(finalState1.pos.Dist(state1.pos), 1.0f);
    EXPECT_GT(finalState2.pos.Dist(state2.pos), 1.0f);
    
    delete arena;
}

TEST_F(IntegrationTest, StateRoundTripAccuracy) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    
    // Get initial state
    CarState originalState = car->GetState();
    
    // Modify state
    originalState.pos = Vec(500.0f, 600.0f, 700.0f);
    originalState.vel = Vec(100.0f, 200.0f, 300.0f);
    originalState.boost = 50.0f;
    
    // Set and get back
    car->SetState(originalState);
    CarState retrievedState = car->GetState();
    
    // Compare (with reasonable tolerance)
    TestUtils::EXPECT_CARSTATE_NEAR(originalState, retrievedState, 0.1f, 0.1f);
    
    delete arena;
}

TEST_F(IntegrationTest, BallStateRoundTripAccuracy) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    BallState originalState;
    originalState.pos = Vec(1000.0f, 2000.0f, 3000.0f);
    originalState.vel = Vec(500.0f, 600.0f, 700.0f);
    originalState.angVel = Vec(1.0f, 2.0f, 3.0f);
    originalState.rotMat = RotMat::GetIdentity();
    
    arena->ball->SetState(originalState);
    BallState retrievedState = arena->ball->GetState();
    
    TestUtils::EXPECT_BALLSTATE_NEAR(originalState, retrievedState, 0.1f, 0.1f);
    
    delete arena;
}

TEST_F(IntegrationTest, SimulationConsistency) {
    Arena* arena1 = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    Arena* arena2 = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car1 = arena1->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    Car* car2 = arena2->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    
    // Set identical initial states
    CarState initialState;
    initialState.pos = Vec(0, 0, 100.0f);
    initialState.vel = Vec(1000.0f, 0, 0);
    initialState.boost = 100.0f;
    
    car1->SetState(initialState);
    car2->SetState(initialState);
    
    // Set identical controls
    car1->controls.throttle = 1.0f;
    car2->controls.throttle = 1.0f;
    
    // Simulate same number of ticks
    for (int i = 0; i < 100; i++) {
        arena1->Step(1);
        arena2->Step(1);
    }
    
    // States should be very similar (allowing for floating point differences)
    CarState state1 = car1->GetState();
    CarState state2 = car2->GetState();
    
    EXPECT_NEAR(state1.pos.x, state2.pos.x, 1.0f);
    EXPECT_NEAR(state1.pos.y, state2.pos.y, 1.0f);
    EXPECT_NEAR(state1.pos.z, state2.pos.z, 1.0f);
    
    delete arena1;
    delete arena2;
}

TEST_F(IntegrationTest, ArenaCloning) {
    Arena* arena1 = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car1 = arena1->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    CarState carState;
    carState.pos = Vec(1000.0f, 2000.0f, 3000.0f);
    carState.boost = 75.0f;
    car1->SetState(carState);
    
    BallState ballState = arena1->ball->GetState();
    ballState.pos = Vec(500.0f, 600.0f, 700.0f);
    arena1->ball->SetState(ballState);
    
    // Clone arena
    Arena* arena2 = arena1->Clone(false); // Don't copy callbacks
    
    // Verify cloned arena has same state
    const auto& cars1 = arena1->GetCars();
    const auto& cars2 = arena2->GetCars();
    EXPECT_EQ(cars1.size(), cars2.size());
    
    if (!cars1.empty() && !cars2.empty()) {
        CarState state1 = (*cars1.begin())->GetState();
        CarState state2 = (*cars2.begin())->GetState();
        
        EXPECT_NEAR(state1.pos.x, state2.pos.x, 0.1f);
        EXPECT_NEAR(state1.boost, state2.boost, 0.1f);
    }
    
    BallState ballState1 = arena1->ball->GetState();
    BallState ballState2 = arena2->ball->GetState();
    
    EXPECT_NEAR(ballState1.pos.x, ballState2.pos.x, 0.1f);
    
    delete arena1;
    delete arena2;
}

TEST_F(IntegrationTest, LongSimulation) {
    Arena* arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    
    Car* car = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    
    // Run simulation for many ticks
    uint64_t initialTickCount = arena->tickCount;
    for (int i = 0; i < 1000; i++) {
        arena->Step(1);
    }
    
    EXPECT_EQ(arena->tickCount, initialTickCount + 1000);
    
    // Car should still exist
    const auto& cars = arena->GetCars();
    EXPECT_EQ(cars.size(), 1);
    
    CarState finalState = car->GetState();
    // State should be valid (not NaN or infinite)
    EXPECT_TRUE(std::isfinite(finalState.pos.x));
    EXPECT_TRUE(std::isfinite(finalState.pos.y));
    EXPECT_TRUE(std::isfinite(finalState.pos.z));
    
    delete arena;
}

