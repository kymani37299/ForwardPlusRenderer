#pragma once

namespace MathUtility
{
	// ceil(a/b)
	inline uint32_t CeilDiv(uint32_t a, uint32_t b) { return (a + b - 1) / b; }

	inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
}