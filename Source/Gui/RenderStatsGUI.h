#pragma once

#include "Gui/GUI.h"

struct RenderStatistics
{
	uint32_t TotalLights;
	uint32_t VisibleLights;

	uint32_t TotalDrawables;
	uint32_t VisibleDrawables;
};

class RenderStatsGUI : public GUIElement
{
public:
	RenderStatsGUI() : GUIElement("Render stats") {}

	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);
};

extern RenderStatistics RenderStats;