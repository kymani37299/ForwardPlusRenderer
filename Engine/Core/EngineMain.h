#pragma once

#include <time.h>

#include "Core/Engine.h"
#include "Core/Application.h"
#include "System/ApplicationConfiguration.h"
#include "System/VSConsoleRedirect.h"
#include "Utility/StringUtility.h"

// Implement on client
Application* Main(ApplicationConfiguration& appConfig);

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
	ReadCommandArguments(std::string(cmdParams));

	RedirectToVSConsoleScoped _vsConsoleRedirect;

	srand((uint32_t)time(0));

	Engine e{ Main(AppConfig) };
	e.Run();
}