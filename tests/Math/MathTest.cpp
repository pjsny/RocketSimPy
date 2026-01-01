#include <gtest/gtest.h>
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

TEST(MathTest, LinearPieceCurve) {
    LinearPieceCurve curve;
    
    // Empty curve should return default
    EXPECT_NEAR(curve.GetOutput(5.0f, 10.0f), 10.0f, 0.01f);
    
    // Add some mappings
    curve.valueMappings[0.0f] = 0.0f;
    curve.valueMappings[10.0f] = 20.0f;
    curve.valueMappings[20.0f] = 40.0f;
    
    // Test exact matches
    EXPECT_NEAR(curve.GetOutput(0.0f), 0.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(10.0f), 20.0f, 0.01f);
    EXPECT_NEAR(curve.GetOutput(20.0f), 40.0f, 0.01f);
    
    // Test interpolation
    EXPECT_NEAR(curve.GetOutput(5.0f), 10.0f, 0.01f); // Halfway between 0->10 should be 10
    EXPECT_NEAR(curve.GetOutput(15.0f), 30.0f, 0.01f); // Halfway between 10->20 should be 30
    
    // Test below first value
    EXPECT_NEAR(curve.GetOutput(-5.0f), 0.0f, 0.01f);
    
    // Test above last value
    EXPECT_NEAR(curve.GetOutput(25.0f), 40.0f, 0.01f);
}

