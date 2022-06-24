#pragma once

#include "Utility/Timer.h"

class Application;

class Engine
{
public:
	Engine(Application* app);
	~Engine();

	void Run();
	void ReloadShaders();

private:
	Application* m_Application;
	Timer m_FrameTimer;
};