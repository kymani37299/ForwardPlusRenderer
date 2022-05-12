#pragma once

#include "Gui/GUI.h"

struct PostprocessingSettings
{
	bool EnableTAA = true;
};

class PostprocessingGUI : public GUIElement
{
public:
	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);

private:
	bool m_Shown = true;
};

extern PostprocessingSettings PostprocessSettings;