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
	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);

private:
	bool m_Shown = true;
};

extern RenderStatistics RenderStats;