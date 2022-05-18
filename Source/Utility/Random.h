#pragma once

#include <stdlib.h>
#include <math.h>

namespace Random
{
	// Deterministic random float [0,1]
	float UNorm(float seed)
	{
		static float intPart;
		return (modf(sin(seed * 321451.3124f), &intPart) * 43758.5453f);
	}

	// Random float [0,1]
	float UNorm()
	{
		return (float) rand() / RAND_MAX;
	}

	// Random float [-1, 1]
	float SNorm()
	{
		return UNorm() * 2.0f - 1.0f;
	}

	// Random float [a,b]
	float Float(float a, float b)
	{
		return UNorm() * (b - a) + a;
	}
}