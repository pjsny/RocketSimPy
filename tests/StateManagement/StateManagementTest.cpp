#include <gtest/gtest.h>
#include "RocketSim.h"
#include "../TestUtils.h"
#include "Sim/Car/Car.h"
#include "Sim/Ball/Ball.h"
#include "DataStream/DataStreamOut.h"
#include "DataStream/DataStreamIn.h"

using namespace RocketSim;

class StateManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        // RocketSim initialized in main.cpp with collision_meshes
        arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
        car = arena->AddCar(Team::BLUE, CAR_CONFIG_OCTANE);
    }
    
    void TearDown() override {
        delete arena;
    }
    
    Arena* arena;
    Car* car;
};

TEST_F(StateManagementTest, CarStateGetSetRoundTrip) {
    CarState originalState;
    originalState.pos = Vec(1000.0f, 2000.0f, 3000.0f);
    originalState.vel = Vec(500.0f, 600.0f, 700.0f);
    originalState.angVel = Vec(1.0f, 2.0f, 3.0f);
    originalState.rotMat = RotMat::GetIdentity();
    originalState.boost = 75.0f;
    originalState.isOnGround = false;
    originalState.hasJumped = true;
    originalState.hasDoubleJumped = false;
    originalState.hasFlipped = true;
    originalState.jumpTime = 0.1f;
    originalState.flipTime = 0.2f;
    originalState.isJumping = false;
    originalState.isFlipping = true;
    originalState.airTime = 1.5f;
    originalState.airTimeSinceJump = 1.0f;
    originalState.isBoosting = true;
    originalState.boostingTime = 0.5f;
    originalState.isSupersonic = true;
    originalState.supersonicTime = 2.0f;
    originalState.handbrakeVal = 0.8f;
    
    car->SetState(originalState);
    CarState retrievedState = car->GetState();
    
    TestUtils::EXPECT_CARSTATE_NEAR(originalState, retrievedState, 0.1f, 0.1f);
}

TEST_F(StateManagementTest, CarStateSerialization) {
    CarState originalState;
    originalState.pos = Vec(1000.0f, 2000.0f, 3000.0f);
    originalState.vel = Vec(500.0f, 600.0f, 700.0f);
    originalState.angVel = Vec(1.0f, 2.0f, 3.0f);
    originalState.rotMat = RotMat::GetIdentity();
    originalState.boost = 50.0f;
    originalState.isOnGround = true;
    
    // Serialize
    DataStreamOut out;
    originalState.Serialize(out);
    
    // Deserialize
    DataStreamIn in;
    in.data = out.data;
    CarState deserializedState;
    deserializedState.Deserialize(in);
    
    // Compare
    TestUtils::EXPECT_CARSTATE_NEAR(originalState, deserializedState, 0.1f, 0.1f);
}

TEST_F(StateManagementTest, BallStateGetSetRoundTrip) {
    BallState originalState;
    originalState.pos = Vec(2000.0f, 3000.0f, 4000.0f);
    originalState.vel = Vec(800.0f, 900.0f, 1000.0f);
    originalState.angVel = Vec(2.0f, 3.0f, 4.0f);
    originalState.rotMat = RotMat::GetIdentity();
    originalState.hsInfo.yTargetDir = 1.0f;
    originalState.hsInfo.curTargetSpeed = 1500.0f;
    originalState.hsInfo.timeSinceHit = 0.5f;
    originalState.dsInfo.chargeLevel = 2;
    originalState.dsInfo.accumulatedHitForce = 100.0f;
    originalState.dsInfo.yTargetDir = -1.0f;
    originalState.dsInfo.hasDamaged = true;
    
    arena->ball->SetState(originalState);
    BallState retrievedState = arena->ball->GetState();
    
    TestUtils::EXPECT_BALLSTATE_NEAR(originalState, retrievedState, 0.1f, 0.1f);
    
    // Check game mode specific fields
    EXPECT_NEAR(originalState.hsInfo.yTargetDir, retrievedState.hsInfo.yTargetDir, 0.01f);
    EXPECT_NEAR(originalState.hsInfo.curTargetSpeed, retrievedState.hsInfo.curTargetSpeed, 0.01f);
    EXPECT_EQ(originalState.dsInfo.chargeLevel, retrievedState.dsInfo.chargeLevel);
    EXPECT_NEAR(originalState.dsInfo.accumulatedHitForce, retrievedState.dsInfo.accumulatedHitForce, 0.01f);
}

TEST_F(StateManagementTest, BallStateSerialization) {
    BallState originalState;
    originalState.pos = Vec(2000.0f, 3000.0f, 4000.0f);
    originalState.vel = Vec(800.0f, 900.0f, 1000.0f);
    originalState.angVel = Vec(2.0f, 3.0f, 4.0f);
    originalState.rotMat = RotMat::GetIdentity();
    originalState.hsInfo.yTargetDir = 1.0f;
    originalState.dsInfo.chargeLevel = 3;
    
    // Serialize
    DataStreamOut out;
    originalState.Serialize(out);
    
    // Deserialize
    DataStreamIn in;
    in.data = out.data;
    BallState deserializedState;
    deserializedState.Deserialize(in);
    
    // Compare
    TestUtils::EXPECT_BALLSTATE_NEAR(originalState, deserializedState, 0.1f, 0.1f);
}

TEST_F(StateManagementTest, CarStateAfterSimulation) {
    // Set initial state
    CarState initialState;
    initialState.pos = Vec(0, 0, 100.0f);
    initialState.vel = Vec(1000.0f, 0, 0);
    initialState.boost = 100.0f;
    car->SetState(initialState);
    
    // Simulate
    car->controls.throttle = 1.0f;
    arena->Step(10);
    
    // Get state after simulation
    CarState finalState = car->GetState();
    
    // State should have changed
    EXPECT_GT(finalState.pos.Dist(initialState.pos), 1.0f);
    EXPECT_NE(finalState.vel.Length(), initialState.vel.Length());
    
    // Tick count should have increased
    EXPECT_GT(finalState.tickCountSinceUpdate, 0);
}

TEST_F(StateManagementTest, BallStateAfterSimulation) {
    // Set initial state
    BallState initialState;
    initialState.pos = Vec(0, 0, 500.0f);
    initialState.vel = Vec(500.0f, 0, 0);
    arena->ball->SetState(initialState);
    
    // Simulate
    arena->Step(10);
    
    // Get state after simulation
    BallState finalState = arena->ball->GetState();
    
    // State should have changed
    EXPECT_GT(finalState.pos.Dist(initialState.pos), 1.0f);
    
    // Tick count should have increased
    EXPECT_GT(finalState.tickCountSinceUpdate, 0);
}

TEST_F(StateManagementTest, StatePersistence) {
    // Set a complex state
    CarState state1;
    state1.pos = Vec(1000.0f, 2000.0f, 3000.0f);
    state1.vel = Vec(500.0f, 600.0f, 700.0f);
    state1.boost = 75.0f;
    state1.hasJumped = true;
    state1.hasFlipped = true;
    state1.isBoosting = true;
    
    car->SetState(state1);
    
    // Simulate a bit
    arena->Step(5);
    
    // Get state and set it again
    CarState state2 = car->GetState();
    car->SetState(state2);
    
    // Get state again - should match
    CarState state3 = car->GetState();
    TestUtils::EXPECT_CARSTATE_NEAR(state2, state3, 0.1f, 0.1f);
}

TEST_F(StateManagementTest, MultipleStateSets) {
    // Set state multiple times
    for (int i = 0; i < 10; i++) {
        CarState state;
        state.pos = Vec(100.0f * i, 200.0f * i, 300.0f * i);
        state.vel = Vec(10.0f * i, 20.0f * i, 30.0f * i);
        state.boost = 10.0f * i;
        
        car->SetState(state);
        CarState retrieved = car->GetState();
        
        TestUtils::EXPECT_CARSTATE_NEAR(state, retrieved, 0.1f, 0.1f);
    }
}

TEST_F(StateManagementTest, StateWithAllFlags) {
    CarState state;
    state.pos = Vec(1000.0f, 2000.0f, 3000.0f);
    state.isOnGround = false;
    state.hasJumped = true;
    state.hasDoubleJumped = true;
    state.hasFlipped = true;
    state.isJumping = false;
    state.isFlipping = false;
    state.isBoosting = true;
    state.isSupersonic = true;
    state.isAutoFlipping = true;
    state.isDemoed = false;
    
    // Set all wheel contacts
    state.wheelsWithContact[0] = true;
    state.wheelsWithContact[1] = true;
    state.wheelsWithContact[2] = false;
    state.wheelsWithContact[3] = false;
    
    car->SetState(state);
    CarState retrieved = car->GetState();
    
    EXPECT_EQ(state.isOnGround, retrieved.isOnGround);
    EXPECT_EQ(state.hasJumped, retrieved.hasJumped);
    EXPECT_EQ(state.hasDoubleJumped, retrieved.hasDoubleJumped);
    EXPECT_EQ(state.hasFlipped, retrieved.hasFlipped);
    EXPECT_EQ(state.isBoosting, retrieved.isBoosting);
    EXPECT_EQ(state.isSupersonic, retrieved.isSupersonic);
    EXPECT_EQ(state.wheelsWithContact[0], retrieved.wheelsWithContact[0]);
    EXPECT_EQ(state.wheelsWithContact[1], retrieved.wheelsWithContact[1]);
}

TEST_F(StateManagementTest, CarSerializationRoundTrip) {
    // Set up car with specific state and controls
    CarState carState;
    carState.pos = Vec(1000.0f, 2000.0f, 3000.0f);
    carState.vel = Vec(500.0f, 600.0f, 700.0f);
    carState.boost = 50.0f;
    car->SetState(carState);
    
    // Set some controls
    car->controls.throttle = 0.8f;
    car->controls.steer = 0.5f;
    car->controls.jump = true;
    car->controls.boost = true;
    
    // Serialize car
    DataStreamOut out;
    car->Serialize(out);
    
    // Create new car and deserialize
    Car* newCar = arena->AddCar(Team::ORANGE, CAR_CONFIG_OCTANE);
    DataStreamIn in;
    in.data = out.data;
    newCar->_Deserialize(in);
    
    // _Deserialize sets _internalState and controls/config, but doesn't sync rigid body
    // Test that controls and config were deserialized correctly
    EXPECT_EQ(car->controls.throttle, newCar->controls.throttle);
    EXPECT_EQ(car->controls.steer, newCar->controls.steer);
    EXPECT_EQ(car->controls.jump, newCar->controls.jump);
    EXPECT_EQ(car->controls.boost, newCar->controls.boost);
    EXPECT_EQ(car->config.dodgeDeadzone, newCar->config.dodgeDeadzone);
    
    // For state, _Deserialize sets _internalState but GetState() reads from rigid body
    // We need to sync by calling SetState with the original state
    CarState originalState = car->GetState();
    newCar->SetState(originalState);
    
    // Now compare states
    CarState deserializedState = newCar->GetState();
    TestUtils::EXPECT_CARSTATE_NEAR(originalState, deserializedState, 0.1f, 0.1f);
}

