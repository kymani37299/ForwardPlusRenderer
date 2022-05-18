#pragma once

#include "Gui/GUI.h"

class PositionInfoGUI : public GUIElement
{
public:
	PositionInfoGUI() : GUIElement("Position info") {}

	virtual void Update(float dt) {}
	virtual void Render(ID3D11DeviceContext* context);
};