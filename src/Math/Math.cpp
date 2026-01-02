#include "Math.h"

RS_NS_START

float LinearPieceCurve::GetOutput(float input, float defaultOutput) const {
	if (numPoints == 0)
		return defaultOutput;
	
	// Before first point
	if (input <= points[0].input)
		return points[0].output;
	
	// Linear search through points (fast for small N, better cache locality than map)
	for (size_t i = 1; i < numPoints; i++) {
		if (points[i].input > input) {
			// Interpolate between points[i-1] and points[i]
			const Point& before = points[i - 1];
			const Point& after = points[i];
			
			// Guard against division by zero (degenerate segments)
			float dx = after.input - before.input;
			if (dx <= 0.0f)
				return before.output;
			
			float t = (input - before.input) / dx;
			return before.output + (after.output - before.output) * t;
		}
	}
	
	// Beyond last point
	return points[numPoints - 1].output;
}

btVector3 Math::RoundVec(btVector3 vec, float precision) {
	vec.x() = roundf(vec.x() / precision) * precision;
	vec.y() = roundf(vec.y() / precision) * precision;
	vec.z() = roundf(vec.z() / precision) * precision;
	return vec;
}

int Math::RandInt(int min, int max, int seed) {
	if (seed != -1) {
		std::default_random_engine tempEngine = std::default_random_engine(seed);
		return min + (tempEngine() % (max - min));
	} else {
		std::default_random_engine& randEngine = GetRandEngine();
		return min + (randEngine() % (max - min));
	}
}

float Math::RandFloat(float min, float max) {
	std::default_random_engine& randEngine = GetRandEngine();
	return min + ((randEngine() / (float)randEngine.max()) * (max - min));
}

std::default_random_engine& Math::GetRandEngine() {
	static thread_local auto hashThreadID = std::hash<std::thread::id>();
	static thread_local uint64_t seed = RS_CUR_MS() + hashThreadID(std::this_thread::get_id());
	static thread_local std::default_random_engine randEngine = std::default_random_engine(seed);
	return randEngine;
}

float Math::WrapNormalizeFloat(float val, float minmax) {
	float result = fmod(val, minmax * 2);
	if (result > minmax)
		result -= minmax * 2;
	else if (result < -minmax)
		result += minmax * 2;
	return result;
}

Angle Math::RoundAngleUE3(Angle ang) {
	// See: https://unrealarchive.org/wikis/unreal-wiki/Rotator.html
	// You can determine the rounding from measuring the resulting vector directions from conversions
	// This was very, very annoying to figure out :/

	constexpr float TO_INTS = (float)(1 << 15) / M_PI;
	constexpr float BACK_TO_RADIANS = (1.f / TO_INTS) * 4.f;
	constexpr int ROUNDING_MASK = 0x4000 - 1;

	int rYaw = ((int)(ang.yaw * TO_INTS) >> 2) & ROUNDING_MASK;
	int rPitch = ((int)(ang.pitch * TO_INTS) >> 2) & ROUNDING_MASK;
	ang.yaw = rYaw * BACK_TO_RADIANS;
	ang.pitch = rPitch * BACK_TO_RADIANS;
	assert(ang.roll == 0);
	
	return ang;
}

RS_NS_END