#pragma once

#include "Gui/GUI.h"

class FPSCounterGUI : public GUIElement
{
public:
	FPSCounterGUI() : GUIElement("Fps counter") {}

	virtual void Update(float dt);
	virtual void Render(ID3D11DeviceContext* context);

private:
	float m_CurrentDT = 0.0f;
};