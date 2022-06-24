#include "Input.h"

#include "System/Window.h"

namespace Input
{
	bool IsKeyPressed(unsigned int key)
	{
		return WindowInput::IsKeyPressed(key);
	}

	bool IsKeyJustPressed(unsigned int key)
	{
		return WindowInput::IsKeyJustPressed(key);
	}

	Float2 GetMousePos()
	{
		return WindowInput::GetMousePos();
	}

	Float2 GetMouseDelta()
	{
		return WindowInput::GetMouseDelta();
	}
}