#pragma once

#include "Utility/Timer.h"

class Renderer;

class Engine
{
public:
	Engine();
	~Engine();

	void Run();

private:
	void UpdateInput(float dt);

private:
	Renderer* m_Renderer;
	Timer m_FrameTimer;
};