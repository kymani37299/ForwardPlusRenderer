#pragma once

#include "Utility/DataTypes.h"

namespace MathUtility
{
	// ceil(a/b)
	inline uint32_t CeilDiv(uint32_t a, uint32_t b) { return (a + b - 1) / b; }

	template<typename T>
	T Lerp(const T& a, const T& b, float t) { return a + t * (b - a); }

	template<typename T>
	inline T Slerp(T a, T b, float t)
	{
		float cosTheta = a.Dot(b);

		if (cosTheta < 0.0f)
		{
			b *= -1.0f;
			cosTheta *= -1.0f;
		}

		// Early exit: If a and b too close we can just use linear interpolation
		constexpr float Epsilon = 0.0005f;
		if (cosTheta > 1.0f - Epsilon) return Lerp(a, b, t);

		const float theta = acos(cosTheta);
		const float sinTheta = sin(theta);

		const float sigma = t * theta;
		const float cosSigma = cos(sigma);
		const float sinSigma = sin(sigma);

		const float factorA = cosSigma - (cosTheta * sinSigma) / sinTheta;
		const float factorB = sinSigma / sinTheta;

		return factorA * a + factorB * b;
	}

	template<>
	inline float Slerp(float a, float b, float t) { return Lerp(a, b, t); }
}