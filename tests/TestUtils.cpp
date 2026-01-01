#include "TestUtils.h"
#include "Sim/Arena/Arena.h"
#include "Sim/Arena/ArenaConfig/ArenaConfig.h"

namespace TestUtils {

RocketSim::Arena* CreateTestArena(RocketSim::GameMode gameMode, float tickRate) {
    RocketSim::ArenaConfig config;
    return RocketSim::Arena::Create(gameMode, config, tickRate);
}

void EXPECT_VEC_NEAR(const RocketSim::Vec& expected, const RocketSim::Vec& actual, float tolerance) {
    EXPECT_NEAR(expected.x, actual.x, tolerance) << "Vec x component";
    EXPECT_NEAR(expected.y, actual.y, tolerance) << "Vec y component";
    EXPECT_NEAR(expected.z, actual.z, tolerance) << "Vec z component";
}

void EXPECT_ROTMAT_NEAR(const RocketSim::RotMat& expected, const RocketSim::RotMat& actual, float tolerance) {
    EXPECT_VEC_NEAR(expected.forward, actual.forward, tolerance);
    EXPECT_VEC_NEAR(expected.right, actual.right, tolerance);
    EXPECT_VEC_NEAR(expected.up, actual.up, tolerance);
}

void EXPECT_CARSTATE_NEAR(const RocketSim::CarState& expected, const RocketSim::CarState& actual, float posTol, float velTol) {
    EXPECT_VEC_NEAR(expected.pos, actual.pos, posTol);
    EXPECT_ROTMAT_NEAR(expected.rotMat, actual.rotMat, 0.01f); // Rotation tolerance
    EXPECT_VEC_NEAR(expected.vel, actual.vel, velTol);
    EXPECT_VEC_NEAR(expected.angVel, actual.angVel, 0.01f);
    
    EXPECT_EQ(expected.isOnGround, actual.isOnGround);
    EXPECT_EQ(expected.hasJumped, actual.hasJumped);
    EXPECT_EQ(expected.hasDoubleJumped, actual.hasDoubleJumped);
    EXPECT_EQ(expected.hasFlipped, actual.hasFlipped);
    EXPECT_NEAR(expected.boost, actual.boost, 0.1f);
    EXPECT_NEAR(expected.jumpTime, actual.jumpTime, 0.01f);
    EXPECT_NEAR(expected.flipTime, actual.flipTime, 0.01f);
}

void EXPECT_BALLSTATE_NEAR(const RocketSim::BallState& expected, const RocketSim::BallState& actual, float posTol, float velTol) {
    EXPECT_VEC_NEAR(expected.pos, actual.pos, posTol);
    EXPECT_ROTMAT_NEAR(expected.rotMat, actual.rotMat, 0.01f);
    EXPECT_VEC_NEAR(expected.vel, actual.vel, velTol);
    EXPECT_VEC_NEAR(expected.angVel, actual.angVel, 0.01f);
}

} // namespace TestUtils

