#pragma once
#include "../BaseInc.h"

RS_NS_START

// Fast linear piecewise curve using sorted array (faster than std::map for small N)
struct LinearPieceCurve {
	struct Point {
		float input;
		float output;
	};
	
	// Store points in a fixed-size array (most curves have <= 6 points)
	static constexpr size_t MAX_POINTS = 8;
	Point points[MAX_POINTS];
	size_t numPoints = 0;
	
	// Constructor from initializer list of {input, output} pairs
	constexpr LinearPieceCurve(std::initializer_list<std::pair<float, float>> init) : points{}, numPoints(0) {
		for (const auto& p : init) {
			if (numPoints < MAX_POINTS) {
				points[numPoints++] = {p.first, p.second};
			}
		}
	}
	
	constexpr LinearPieceCurve() : points{}, numPoints(0) {}

	float GetOutput(float input, float defaultOutput = 1) const;
};

namespace Math {
	btVector3 RoundVec(btVector3 vec, float precision);

	// NOTE: min is inclusive, max is exclusive
	// Seed will be used if not -1
	int RandInt(int min, int max, int seed = -1);

	float RandFloat(float min = 0, float max = 1);

	std::default_random_engine& GetRandEngine();

	float WrapNormalizeFloat(float val, float minmax);

	// Simulates aggressive UE3 rotator rounding when converting from a UE3 rotator to vector
	Angle RoundAngleUE3(Angle ang);
}

RS_NS_END