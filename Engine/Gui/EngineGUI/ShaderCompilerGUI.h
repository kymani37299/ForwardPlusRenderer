#pragma once

#include "Gui/GUI.h"

class ShaderCompilerGUI : public GUIElement
{
public:
	ShaderCompilerGUI() : GUIElement("Shader compiler", GUIFlags::MenuHidden) {}
	virtual void Update(float dt);
	virtual void Render();
};