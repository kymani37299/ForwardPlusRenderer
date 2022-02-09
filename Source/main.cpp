#include <Windows.h>
#include <iostream>

#include "Common.h"
#include "System/VSConsoleRedirect.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

ApplicationConfiguration AppConfig;

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParams, int showFlags)
{
	RedirectToVSConsoleScoped _vsConsoleRedirect;

	AppConfig.AppHandle = instance;
	AppConfig.WindowTitle = "Forward plus graphics";
	AppConfig.WindowWidth = 1024;
	AppConfig.WindowHeight = 768;

	Window::Init();

	while (Window::Get()->IsRunning())
	{
		Window::Get()->Update(0.0f);
	}

	Window::Destroy();
}