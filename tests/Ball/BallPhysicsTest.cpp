#include <gtest/gtest.h>
#include "RocketSim.h"
#include "../TestUtils.h"
#include "Sim/Ball/Ball.h"
#include "RLConst.h"

using namespace RocketSim;

class BallPhysicsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // RocketSim initialized in main.cpp with collision_meshes
        arena = TestUtils::CreateTestArena(GameMode::THE_VOID, 120.0f);
    }
    
    void TearDown() override {
        delete arena;
    }
    
    Arena* arena;
};

TEST_F(BallPhysicsTest, GetSetStateRoundTrip) {
    BallState originalState;
    originalState.pos = Vec(100.0f, 200.0f, 300.0f);
    originalState.vel = Vec(500.0f, 600.0f, 700.0f);
    originalState.angVel = Vec(1.0f, 2.0f, 3.0f);
    originalState.rotMat = RotMat::GetIdentity();
    
    arena->ball->SetState(originalState);
    BallState retrievedState = arena->ball->GetState();
    
    TestUtils::EXPECT_BALLSTATE_NEAR(originalState, retrievedState, 0.1f, 0.1f);
}

TEST_F(BallPhysicsTest, MatchesComparison) {
    BallState state1, state2;
    
    state1.pos = Vec(100.0f, 200.0f, 300.0f);
    state1.vel = Vec(500.0f, 600.0f, 700.0f);
    state1.angVel = Vec(1.0f, 2.0f, 3.0f);
    
    // Use smaller differences to stay within margins
    // marginPos = 0.8, so distance must be < 0.8
    // marginVel = 0.4, so velocity difference must be < 0.4
    // marginAngVel = 0.02, so angular velocity difference must be < 0.02
    state2.pos = Vec(100.3f, 200.3f, 300.3f); // Distance ~0.52, within margin (0.8)
    state2.vel = Vec(500.1f, 600.1f, 700.1f); // Difference ~0.17, within margin (0.4)
    state2.angVel = Vec(1.01f, 2.01f, 3.01f); // Difference ~0.017, within margin (0.02)
    
    EXPECT_TRUE(state1.Matches(state2));
    
    // Test with values outside margin
    state2.pos = Vec(200.0f, 200.0f, 300.0f); // Too far
    EXPECT_FALSE(state1.Matches(state2));
}

TEST_F(BallPhysicsTest, VelocityLimitClamping) {
    BallState state;
    state.pos = Vec(0, 0, RLConst::BALL_REST_Z);
    state.vel = Vec(10000.0f, 0, 0); // Exceeds max speed
    state.angVel = Vec(0, 0, 0);
    state.rotMat = RotMat::GetIdentity();
    
    arena->ball->SetState(state);
    arena->Step(1);
    
    BallState resultState = arena->ball->GetState();
    float speed = resultState.vel.Length();
    EXPECT_LE(speed, RLConst::BALL_MAX_SPEED + 1.0f); // Allow small tolerance
}

TEST_F(BallPhysicsTest, AngularVelocityLimitClamping) {
    BallState state;
    state.pos = Vec(0, 0, RLConst::BALL_REST_Z);
    state.vel = Vec(0, 0, 0);
    state.angVel = Vec(10.0f, 10.0f, 10.0f); // Exceeds max angular speed
    state.rotMat = RotMat::GetIdentity();
    
    arena->ball->SetState(state);
    arena->Step(1);
    
    BallState resultState = arena->ball->GetState();
    float angSpeed = resultState.angVel.Length();
    EXPECT_LE(angSpeed, RLConst::BALL_MAX_ANG_SPEED + 0.1f);
}

TEST_F(BallPhysicsTest, BallStateTickCount) {
    BallState state = arena->ball->GetState();
    uint64_t initialTickCount = state.tickCountSinceUpdate;
    
    arena->Step(1);
    state = arena->ball->GetState();
    EXPECT_EQ(state.tickCountSinceUpdate, initialTickCount + 1);
    
    arena->Step(5);
    state = arena->ball->GetState();
    EXPECT_EQ(state.tickCountSinceUpdate, initialTickCount + 6);
    
    // Setting state should reset tick count
    BallState newState = state;
    arena->ball->SetState(newState);
    state = arena->ball->GetState();
    EXPECT_EQ(state.tickCountSinceUpdate, 0);
}

TEST_F(BallPhysicsTest, BallRadius) {
    float radius = arena->ball->GetRadius();
    EXPECT_GT(radius, 0.0f);
    EXPECT_LT(radius, 200.0f); // Reasonable upper bound
    
    // Test that radius matches expected value for SOCCAR (in THE_VOID, should default to SOCCAR)
    EXPECT_NEAR(radius, RLConst::BALL_COLLISION_RADIUS_SOCCAR, 1.0f);
}

TEST_F(BallPhysicsTest, BallMass) {
    float mass = arena->ball->GetMass();
    EXPECT_GT(mass, 0.0f);
    EXPECT_NEAR(mass, RLConst::BALL_MASS_BT, 0.1f);
}

TEST_F(BallPhysicsTest, BallIsSphere) {
    // In THE_VOID mode, ball should be a sphere
    EXPECT_TRUE(arena->ball->IsSphere());
}

TEST_F(BallPhysicsTest, BallPhysicsStep) {
    BallState initialState = arena->ball->GetState();
    
    // Give ball some velocity
    initialState.vel = Vec(1000.0f, 0, 0);
    arena->ball->SetState(initialState);
    
    // Step simulation
    arena->Step(10);
    
    BallState finalState = arena->ball->GetState();
    
    // Position should have changed
    EXPECT_GT(finalState.pos.Dist(initialState.pos), 1.0f);
    
    // Velocity should have decreased due to drag (in THE_VOID, but drag still applies)
    // Note: In THE_VOID without ground, ball will fall, so Z velocity will change
}

TEST_F(BallPhysicsTest, HeatseekerInfo) {
    // HEATSEEKER mode requires collision meshes which we don't have in tests
    // Instead, test that we can manually set and get Heatseeker info in THE_VOID mode
    BallState state = arena->ball->GetState();
    
    // Manually set Heatseeker info
    state.hsInfo.yTargetDir = 1.0f;
    state.hsInfo.curTargetSpeed = 1500.0f;
    state.hsInfo.timeSinceHit = 0.5f;
    
    arena->ball->SetState(state);
    BallState retrieved = arena->ball->GetState();
    
    EXPECT_EQ(retrieved.hsInfo.yTargetDir, 1.0f);
    EXPECT_NEAR(retrieved.hsInfo.curTargetSpeed, 1500.0f, 0.1f);
    EXPECT_NEAR(retrieved.hsInfo.timeSinceHit, 0.5f, 0.1f);
}

TEST_F(BallPhysicsTest, DropshotInfo) {
    // DROPSHOT mode requires collision meshes which we don't have in tests
    // Instead, test that we can manually set and get Dropshot info in THE_VOID mode
    BallState state = arena->ball->GetState();
    
    // Manually set Dropshot info
    state.dsInfo.chargeLevel = 2;
    state.dsInfo.accumulatedHitForce = 100.0f;
    state.dsInfo.yTargetDir = -1.0f;
    state.dsInfo.hasDamaged = true;
    
    arena->ball->SetState(state);
    BallState retrieved = arena->ball->GetState();
    
    EXPECT_EQ(retrieved.dsInfo.chargeLevel, 2);
    EXPECT_NEAR(retrieved.dsInfo.accumulatedHitForce, 100.0f, 0.1f);
    EXPECT_EQ(retrieved.dsInfo.yTargetDir, -1.0f);
    EXPECT_TRUE(retrieved.dsInfo.hasDamaged);
}

TEST_F(BallPhysicsTest, BallStateSerializationFields) {
    BallState state;
    state.pos = Vec(100.0f, 200.0f, 300.0f);
    state.vel = Vec(500.0f, 600.0f, 700.0f);
    state.angVel = Vec(1.0f, 2.0f, 3.0f);
    state.rotMat = RotMat::GetIdentity();
    state.hsInfo.yTargetDir = 1.0f;
    state.hsInfo.curTargetSpeed = 1000.0f;
    state.dsInfo.chargeLevel = 2;
    state.dsInfo.accumulatedHitForce = 50.0f;
    
    arena->ball->SetState(state);
    BallState retrieved = arena->ball->GetState();
    
    // Verify serialization fields are preserved
    EXPECT_NEAR(state.hsInfo.yTargetDir, retrieved.hsInfo.yTargetDir, 0.01f);
    EXPECT_NEAR(state.hsInfo.curTargetSpeed, retrieved.hsInfo.curTargetSpeed, 0.01f);
    EXPECT_EQ(state.dsInfo.chargeLevel, retrieved.dsInfo.chargeLevel);
    EXPECT_NEAR(state.dsInfo.accumulatedHitForce, retrieved.dsInfo.accumulatedHitForce, 0.01f);
}

