#include <Engine/Core/EngineMain.h>

#include "ForwardPlus.h"

Application* Main(ApplicationConfiguration& appConfig)
{
	AppConfig.WindowTitle = "Forward plus graphics";
	AppConfig.WindowWidth = 1024;
	AppConfig.WindowHeight = 768;

	return new ForwardPlus();
}