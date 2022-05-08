#pragma once

namespace MathUtility
{
	// ceil(a/b)
	uint32_t CeilDiv(uint32_t a, uint32_t b) { return (a + b - 1) / b; }
}