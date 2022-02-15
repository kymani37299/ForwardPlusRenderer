#include <Windows.h>
#include <iostream>
#include <time.h>

#include "Core/Engine.h"
#include "System/VSConsoleRedirect.h"
#include "System/ApplicationConfiguration.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParams, int showFlags)
{
	AppConfig.AppHandle = instance;
	AppConfig.WindowTitle = "Forward plus graphics";
	AppConfig.WindowWidth = 1024;
	AppConfig.WindowHeight = 768;

	RedirectToVSConsoleScoped _vsConsoleRedirect;

	srand((uint32_t)time(0));
	
	Engine e;
	e.Run();
}