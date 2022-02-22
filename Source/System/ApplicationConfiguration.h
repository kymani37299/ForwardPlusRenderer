#pragma once

#include <unordered_set>

#include "Common.h"

struct ApplicationConfiguration
{
	// Window initialization
	void* AppHandle = nullptr;
	uint32_t WindowWidth = 0;
	uint32_t WindowHeight = 0;
	std::string WindowTitle = "";
	bool VSyncEnabled = false;
	bool WindowSizeDirty = false;
	
	// Command line settings
	std::unordered_set<std::string> Settings;
};

extern ApplicationConfiguration AppConfig;