#pragma once

#include "Common.h"

struct ApplicationConfiguration
{
	void* AppHandle = nullptr;
	uint32_t WindowWidth = 0;
	uint32_t WindowHeight = 0;
	std::string WindowTitle = "";
	bool VSyncEnabled = false;
	bool WindowSizeDirty = false;
};

extern ApplicationConfiguration AppConfig;