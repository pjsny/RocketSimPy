#pragma once
#include "RocketSim.h"
#include <gtest/gtest.h>
#include <cmath>

namespace TestUtils {

// Tolerance for floating point comparisons
constexpr float FLOAT_TOL = 1e-5f;
constexpr float POS_TOL = 0.1f;  // Position tolerance in UU
constexpr float VEL_TOL = 0.1f;   // Velocity tolerance in UU/s

// Create a test arena without collision meshes (uses THE_VOID mode)
RocketSim::Arena* CreateTestArena(RocketSim::GameMode gameMode = RocketSim::GameMode::THE_VOID, float tickRate = 120.0f);

// Compare two Vec objects with tolerance
void EXPECT_VEC_NEAR(const RocketSim::Vec& expected, const RocketSim::Vec& actual, float tolerance = FLOAT_TOL);

// Compare two RotMat objects with tolerance
void EXPECT_ROTMAT_NEAR(const RocketSim::RotMat& expected, const RocketSim::RotMat& actual, float tolerance = FLOAT_TOL);

// Compare two CarState objects with tolerance
void EXPECT_CARSTATE_NEAR(const RocketSim::CarState& expected, const RocketSim::CarState& actual, float posTol = POS_TOL, float velTol = VEL_TOL);

// Compare two BallState objects with tolerance
void EXPECT_BALLSTATE_NEAR(const RocketSim::BallState& expected, const RocketSim::BallState& actual, float posTol = POS_TOL, float velTol = VEL_TOL);

// Check if a value is near another value
inline bool IsNear(float a, float b, float tolerance = FLOAT_TOL) {
    return std::abs(a - b) < tolerance;
}

} // namespace TestUtils

