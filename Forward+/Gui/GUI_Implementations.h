#pragma once

#include <Engine/Gui/GUI.h>

#include "Globals.h"

class DebugToolsGUI : public GUIElement
{
public:
	DebugToolsGUI(bool shown = false) : GUIElement("Debug tools", shown) {}
	virtual void Update(float dt) {}
	virtual void Render();
};

class PositionInfoGUI : public GUIElement
{
public:
	PositionInfoGUI(bool shown = false) : GUIElement("Position info", shown) {}

	virtual void Update(float dt) {}
	virtual void Render();
};

class RenderSettingsGUI : public GUIElement
{
public:
	RenderSettingsGUI(bool shown = false) : GUIElement("Render settings", shown) {}

	virtual void Update(float dt) {}
	virtual void Render();
};

class RenderStatsGUI : public GUIElement
{
public:
	RenderStatsGUI(bool shown = false) : GUIElement("Render stats", shown) {}

	virtual void Update(float dt);
	virtual void Render();

private:
	std::vector<float> DTHistory;
	float m_CurrentDT = 0.0f;
};

class TextureVisualizerGUI : public GUIElement
{
public:
	TextureVisualizerGUI() : GUIElement("Texture visualizer", false) {}

	virtual void Update(float dt) {}
	virtual void Render();

private:
	uint32_t m_SelectedTexture = 0;
	float m_ScaleFactor = 0.1f;
};

class LightsGUI : public GUIElement
{
public:
	LightsGUI() : GUIElement("Lights", false) {}

	virtual void Update(float dt) {}
	virtual void Render();
};