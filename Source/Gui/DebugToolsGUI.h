#pragma once

#include "Gui/GUI.h"

struct DebugToolsConfiguration
{
	bool DisableGeometryCulling = false;
	bool FreezeGeometryCulling = false;
	bool FreezeLightCulling = false;
	bool DisableLightCulling = false;
	bool DrawBoundingBoxes = false;
};

class DebugToolsGUI : public GUIElement
{
public:
	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);

private:
	bool m_Shown = true;
};

extern DebugToolsConfiguration DebugToolsConfig;