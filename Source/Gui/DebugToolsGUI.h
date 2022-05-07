#pragma once

#include "Gui/GUI.h"

struct DebugToolsConfiguration
{
	bool FreezeCulling = false;
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