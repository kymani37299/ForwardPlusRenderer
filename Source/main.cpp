#include <Windows.h>
#include <iostream>

#include "Common.h"
#include "System/VSConsoleRedirect.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

ApplicationConfiguration AppConfig;

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParams, int showFlags)
{
	AppConfig.AppHandle = instance;
	AppConfig.WindowWidth = 1024;
	AppConfig.WindowHeight = 768;

	Window::Init();

	RedirectToVSConsoleScoped _vsConsoleRedirect;
	std::cout << "Hello world!" << std::endl;

	Window::Destroy();
}