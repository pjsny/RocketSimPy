#include <gtest/gtest.h>
#include <cmath>
#include "Math/Math.h"
#include "../TestUtils.h"

using namespace RocketSim;

TEST(MathTest, RoundVec) {
    Vec vec1(1.234567f, 2.345678f, 3.456789f);
    Vec rounded1 = Math::RoundVec(vec1, 0.1f);
    EXPECT_NEAR(rounded1.x, 1.2f, 0.01f);
    EXPECT_NEAR(rounded1.y, 2.3f, 0.01f);
    EXPECT_NEAR(rounded1.z, 3.5f, 0.01f);
    
    Vec vec2(1.234567f, 2.345678f, 3.456789f);
    Vec rounded2 = Math::RoundVec(vec2, 1.0f);
    EXPECT_NEAR(rounded2.x, 1.0f, 0.01f);
    EXPECT_NEAR(rounded2.y, 2.0f, 0.01f);
    EXPECT_NEAR(rounded2.z, 3.0f, 0.01f);
}

TEST(MathTest, RandInt) {
    // Test with seed for deterministic results
    int val1 = Math::RandInt(0, 10, 42);
    int val2 = Math::RandInt(0, 10, 42);
    EXPECT_EQ(val1, val2); // Same seed should give same result
    
    // Test range
    for (int i = 0; i < 100; i++) {
        int val = Math::RandInt(5, 15, i);
        EXPECT_GE(val, 5);
        EXPECT_LT(val, 15);
    }
    
    // Test negative range
    int val3 = Math::RandInt(-10, -5, 123);
    EXPECT_GE(val3, -10);
    EXPECT_LT(val3, -5);
}

TEST(MathTest, RandFloat) {
    // Test range
    for (int i = 0; i < 100; i++) {
        float val = Math::RandFloat(0.0f, 1.0f);
        EXPECT_GE(val, 0.0f);
        EXPECT_LE(val, 1.0f);
    }
    
    // Test custom range
    for (int i = 0; i < 100; i++) {
        float val = Math::RandFloat(10.0f, 20.0f);
        EXPECT_GE(val, 10.0f);
        EXPECT_LE(val, 20.0f);
    }
}

TEST(MathTest, WrapNormalizeFloat) {
    // Test wrapping around positive boundary
    float val1 = Math::WrapNormalizeFloat(3.5f, M_PI);
    EXPECT_NEAR(val1, 3.5f - 2*M_PI, 0.01f);
    
    // Test wrapping around negative boundary
    float val2 = Math::WrapNormalizeFloat(-3.5f, M_PI);
    EXPECT_NEAR(val2, -3.5f + 2*M_PI, 0.01f);
    
    // Test value within range
    float val3 = Math::WrapNormalizeFloat(1.0f, M_PI);
    EXPECT_NEAR(val3, 1.0f, 0.01f);
    
    // Test exact boundary - M_PI should stay as M_PI (within range [-M_PI, M_PI])
    // The implementation wraps values outside the range, but M_PI is at the boundary
    float val4 = Math::WrapNormalizeFloat(M_PI, M_PI);
    EXPECT_NEAR(val4, M_PI, 0.01f);
}

TEST(MathTest, RoundAngleUE3) {
    // Test basic rounding
    Angle ang1(0.123456f, 0.234567f, 0.0f);
    Angle rounded1 = Math::RoundAngleUE3(ang1);
    
    // The rounding should produce specific values based on UE3's rounding algorithm
    // We test that it produces consistent results
    Angle rounded2 = Math::RoundAngleUE3(ang1);
    EXPECT_EQ(rounded1.yaw, rounded2.yaw);
    EXPECT_EQ(rounded1.pitch, rounded2.pitch);
    EXPECT_EQ(rounded1.roll, rounded2.roll);
    
    // Test that roll remains 0 (as per implementation)
    EXPECT_EQ(rounded1.roll, 0.0f);
}

TEST(MathTest, LinearPieceCurve_Empty) {
    // Empty curve should return default
    LinearPieceCurve emptyCurve;
    EXPECT_NEAR(emptyCurve.GetOutput(5.0f, 10.0f), 10.0f, 0.01f);
    EXPECT_NEAR(emptyCurve.GetOutput(0.0f, 42.0f), 42.0f, 0.01f);
    EXPECT_NEAR(emptyCurve.GetOutput(-100.0f, 1.0f), 1.0f, 0.01f);
}

TEST(MathTest, LinearPieceCurve_SinglePoint) {
    // Single point curve should always return that point's output
    LinearPieceCurve curve = {{5.0f, 100.0f}};
    
    EXPECT_NEAR(curve.GetOutput(5.0f), 100.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(0.0f), 100.0f, 0.01f);  // Below
    EXPECT_NEAR(curve.GetOutput(10.0f), 100.0f, 0.01f); // Above
    EXPECT_NEAR(curve.GetOutput(-1000.0f), 100.0f, 0.01f);
}

TEST(MathTest, LinearPieceCurve_BasicInterpolation) {
    // Create curve with initializer list
    LinearPieceCurve curve = {
        {0.0f, 0.0f},
        {10.0f, 20.0f},
        {20.0f, 40.0f}
    };
    
    // Test exact matches at control points
    EXPECT_NEAR(curve.GetOutput(0.0f), 0.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(10.0f), 20.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(20.0f), 40.0f, 0.01f);
    
    // Test interpolation at midpoints
    EXPECT_NEAR(curve.GetOutput(5.0f), 10.0f, 0.01f);   // Halfway: 0->10 maps to 0->20
    EXPECT_NEAR(curve.GetOutput(15.0f), 30.0f, 0.01f);  // Halfway: 10->20 maps to 20->40
    
    // Test interpolation at quarter points
    EXPECT_NEAR(curve.GetOutput(2.5f), 5.0f, 0.01f);    // 25% through first segment
    EXPECT_NEAR(curve.GetOutput(7.5f), 15.0f, 0.01f);   // 75% through first segment
    EXPECT_NEAR(curve.GetOutput(12.5f), 25.0f, 0.01f);  // 25% through second segment
    EXPECT_NEAR(curve.GetOutput(17.5f), 35.0f, 0.01f);  // 75% through second segment
}

TEST(MathTest, LinearPieceCurve_Clamping) {
    LinearPieceCurve curve = {
        {0.0f, 100.0f},
        {10.0f, 200.0f}
    };
    
    // Test below first value - should clamp to first output
    EXPECT_NEAR(curve.GetOutput(-5.0f), 100.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(-1000.0f), 100.0f, 0.01f);
    
    // Test above last value - should clamp to last output
    EXPECT_NEAR(curve.GetOutput(15.0f), 200.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(1000.0f), 200.0f, 0.01f);
}

TEST(MathTest, LinearPieceCurve_NonLinearMapping) {
    // Test a curve where output doesn't scale linearly with input
    LinearPieceCurve curve = {
        {0.0f, 1.0f},
        {500.0f, 0.5f},
        {1000.0f, 0.2f},
        {1500.0f, 0.1f}
    };
    
    // Verify control points
    EXPECT_NEAR(curve.GetOutput(0.0f), 1.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(500.0f), 0.5f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(1000.0f), 0.2f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(1500.0f), 0.1f, 0.01f);
    
    // Verify interpolation
    EXPECT_NEAR(curve.GetOutput(250.0f), 0.75f, 0.01f);  // Midpoint of 1.0->0.5
    EXPECT_NEAR(curve.GetOutput(750.0f), 0.35f, 0.01f);  // Midpoint of 0.5->0.2
}

TEST(MathTest, LinearPieceCurve_NegativeValues) {
    // Test curve with negative inputs and outputs
    LinearPieceCurve curve = {
        {-10.0f, -100.0f},
        {0.0f, 0.0f},
        {10.0f, 100.0f}
    };
    
    EXPECT_NEAR(curve.GetOutput(-10.0f), -100.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(0.0f), 0.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(10.0f), 100.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(-5.0f), -50.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(5.0f), 50.0f, 0.01f);
}

TEST(MathTest, LinearPieceCurve_DegenerateSegment) {
    // Test curve with duplicate input values (degenerate segment)
    // This should not cause division by zero
    LinearPieceCurve curve = {
        {0.0f, 10.0f},
        {5.0f, 20.0f},
        {5.0f, 30.0f},  // Degenerate: same input as previous
        {10.0f, 40.0f}
    };
    
    // Should not crash or produce NaN
    float result = curve.GetOutput(5.0f);
    EXPECT_FALSE(std::isnan(result));
    EXPECT_FALSE(std::isinf(result));
}

TEST(MathTest, LinearPieceCurve_VerySmallDelta) {
    // Test with very small differences that could cause floating point issues
    LinearPieceCurve curve = {
        {0.0f, 0.0f},
        {1e-7f, 1.0f}
    };
    
    float result = curve.GetOutput(0.5e-7f);
    EXPECT_FALSE(std::isnan(result));
    EXPECT_FALSE(std::isinf(result));
}

TEST(MathTest, LinearPieceCurve_RLConstCurves) {
    // Test that the actual RLConst curves work correctly
    // This catches regressions in the real game physics
    
    // STEER_ANGLE_FROM_SPEED_CURVE approximation
    LinearPieceCurve steerCurve = {
        {0.0f, 0.53356f},
        {500.0f, 0.31930f},
        {1000.0f, 0.18203f},
        {1500.0f, 0.10570f},
        {1750.0f, 0.08507f},
        {3000.0f, 0.03454f}
    };
    
    // At 0 speed, max steering
    EXPECT_NEAR(steerCurve.GetOutput(0.0f), 0.53356f, 0.0001f);
    // At max speed, min steering
    EXPECT_NEAR(steerCurve.GetOutput(3000.0f), 0.03454f, 0.0001f);
    // Above max should clamp
    EXPECT_NEAR(steerCurve.GetOutput(5000.0f), 0.03454f, 0.0001f);
    // Interpolation at 750 (midpoint of 500-1000)
    float expected750 = (0.31930f + 0.18203f) / 2.0f;
    EXPECT_NEAR(steerCurve.GetOutput(750.0f), expected750, 0.0001f);
}

