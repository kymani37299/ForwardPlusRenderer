#include <Windows.h>
#include <iostream>

#include "Common.h"
#include "Core/Renderer.h"
#include "System/VSConsoleRedirect.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

ApplicationConfiguration AppConfig;

class AppScope
{
public:
	AppScope()
	{
		Window::Init();
	}

	~AppScope()
	{
		Window::Destroy();
	}
};

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParams, int showFlags)
{
	AppConfig.AppHandle = instance;
	AppConfig.WindowTitle = "Forward plus graphics";
	AppConfig.WindowWidth = 1024;
	AppConfig.WindowHeight = 768;

	AppScope _appScope;
	RedirectToVSConsoleScoped _vsConsoleRedirect;

	Renderer r;
	float dt = 0.0f;

	while (Window::Get()->IsRunning())
	{
		Window::Get()->Update(dt);
		r.Update(dt);
		r.Render();
	}
}