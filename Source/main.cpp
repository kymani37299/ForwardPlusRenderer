#include <Windows.h>
#include <iostream>

#include "Common.h"
#include "System/VSConsoleRedirect.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParams, int showFlags)
{
	RedirectToVSConsoleScoped _vsConsoleRedirect;
	std::cout << "Hello world!" << std::endl;
}