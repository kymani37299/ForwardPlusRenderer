#pragma once

#include "Common.h"

namespace Input
{
	bool IsKeyPressed(unsigned int key);
	bool IsKeyJustPressed(unsigned int key);
	Float2 GetMousePos();
	Float2 GetMouseDelta();
}