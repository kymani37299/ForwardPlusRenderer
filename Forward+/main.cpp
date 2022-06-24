#include <Windows.h>
#include <iostream>
#include <time.h>

#include <Engine/Core/Engine.h>
#include <Engine/System/VSConsoleRedirect.h>
#include <Engine/System/ApplicationConfiguration.h>
#include <Engine/Utility/StringUtility.h>

#include "ForwardPlus.h"

void ReadCommandArguments(const std::string& arguments)
{
	const auto argumentList = StringUtility::Split(arguments, " ");
	for (const auto argument : argumentList)
	{
		std::string finalArgument = StringUtility::ToUpper(argument);
		StringUtility::ReplaceAll(finalArgument, "-", "");
		StringUtility::ReplaceAll(finalArgument, " ", "");
		AppConfig.Settings.insert(finalArgument);
	}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParams, int showFlags)
{
	AppConfig.AppHandle = instance;
	AppConfig.WindowTitle = "Forward plus graphics";
	AppConfig.WindowWidth = 1024;
	AppConfig.WindowHeight = 768;
	ReadCommandArguments(std::string(cmdParams));

	RedirectToVSConsoleScoped _vsConsoleRedirect;

	srand((uint32_t)time(0));
	
	Engine e{new ForwardPlus()};
	e.Run();
}