#pragma once

#include <stdlib.h>
#include <math.h>

namespace Random
{
	// Deterministic random float [0,1]
	inline float UNorm(float seed)
	{
		static float intPart;
		return modf(sin(seed * 321451.3124f), &intPart);
	}

	// Random float [0,1]
	inline float UNorm()
	{
		return (float) rand() / RAND_MAX;
	}

	// Random float [-1, 1]
	inline float SNorm()
	{
		return UNorm() * 2.0f - 1.0f;
	}

	// Random float [a,b]
	inline float Float(float a, float b)
	{
		return UNorm() * (b - a) + a;
	}

	// Random uint [a,b]
	inline uint32_t UInt(uint32_t a, uint32_t b)
	{
		return rand() % (b - a + 1) + a;
	}
}