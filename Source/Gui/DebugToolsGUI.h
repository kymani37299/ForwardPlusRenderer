#pragma once

#include "Gui/GUI.h"

struct DebugToolsConfiguration
{
	bool DisableGeometryCulling = false;
	bool FreezeGeometryCulling = false;
	bool FreezeLightCulling = false;
	bool DisableLightCulling = false;
	bool DrawBoundingBoxes = false;
	bool LightHeatmap = false;
};

class DebugToolsGUI : public GUIElement
{
public:
	DebugToolsGUI() : GUIElement("Debug tools") {}
	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);
};

extern DebugToolsConfiguration DebugToolsConfig;