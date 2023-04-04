#pragma once

#include <Engine/Gui/GUI.h>

#include "Globals.h"

class DebugVisualizationsGUI : public GUIElement
{
public:
	DebugVisualizationsGUI() : GUIElement("Debug visualizations") {}
	virtual void Update(float dt) {}
	virtual void Render(GraphicsContext& context);
};

class PositionInfoGUI : public GUIElement
{
public:
	PositionInfoGUI() : GUIElement("Position info") {}

	virtual void Update(float dt) {}
	virtual void Render(GraphicsContext& context);
};

class RenderSettingsGUI : public GUIElement
{
public:
	RenderSettingsGUI() : GUIElement("Render settings") {}

	virtual void Update(float dt) {}
	virtual void Render(GraphicsContext& context);
};

class RenderStatsGUI : public GUIElement
{
public:
	RenderStatsGUI() : GUIElement("Render stats", GUIFlags::ShowOnStart) {}

	virtual void Update(float dt);
	virtual void Render(GraphicsContext& context);

private:
	std::vector<float> DTHistory;
	float m_CurrentDT = 0.0f;
};

class LightsGUI : public GUIElement
{
public:
	LightsGUI() : GUIElement("Lights") {}

	virtual void Update(float dt) {}
	virtual void Render(GraphicsContext& context);
};

class TextureDebuggerGUI : public GUIElement
{
public:
	TextureDebuggerGUI() : GUIElement("Texture debugger") {}

	virtual void Update(float dt);
	virtual void Render(GraphicsContext& context);
};

class SceneManagerGUI : public GUIElement
{
public:
	SceneManagerGUI() : GUIElement("Scene manager", GUIFlags::ShowOnStart) {}

	virtual void Update(float dt) {}
	virtual void Render(GraphicsContext& context);
};