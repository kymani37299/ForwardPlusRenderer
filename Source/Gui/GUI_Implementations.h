#pragma once

#include "Gui/GUI.h"

class DebugToolsGUI : public GUIElement
{
public:
	DebugToolsGUI(bool shown = false) : GUIElement("Debug tools", shown) {}
	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);
};

class FPSCounterGUI : public GUIElement
{
public:
	FPSCounterGUI(bool shown = false) : GUIElement("Fps counter", shown) {}

	virtual void Update(float dt);
	virtual void Render(ID3D11DeviceContext* context);

private:
	float m_CurrentDT = 0.0f;
};

class PositionInfoGUI : public GUIElement
{
public:
	PositionInfoGUI(bool shown = false) : GUIElement("Position info", shown) {}

	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);
};

class PostprocessingGUI : public GUIElement
{
public:
	PostprocessingGUI(bool shown = false) : GUIElement("Postprocessing", shown) {}

	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);

private:
	bool m_Shown = true;
};

class RenderStatsGUI : public GUIElement
{
public:
	RenderStatsGUI(bool shown = false) : GUIElement("Render stats", shown) {}

	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);
};

struct DebugToolsConfiguration
{
	bool DisableGeometryCulling = false;
	bool FreezeGeometryCulling = false;
	bool DrawBoundingSpheres = false;

	bool DisableLightCulling = true;
	bool FreezeLightCulling = false;
	bool LightHeatmap = false;
	bool DrawLightSpheres = false;
};

struct PostprocessingSettings
{
	bool EnableTAA = false;
};

struct RenderStatistics
{
	uint32_t TotalLights;
	uint32_t VisibleLights;

	uint32_t TotalDrawables;
	uint32_t VisibleDrawables;
};

extern RenderStatistics RenderStats;
extern PostprocessingSettings PostprocessSettings;
extern DebugToolsConfiguration DebugToolsConfig;