#include "ShaderCompilerGUI.h"

#include "Gui/ImGui_Core.h"

#include "Render/Shader.h"

void ShaderCompilerGUI::Update(float dt)
{
	GetShownRef() = GFX::GetFailedShaderCount() != 0;
}

void ShaderCompilerGUI::Render()
{
	ImGui::Text("Number of failed shaders: %u", GFX::GetFailedShaderCount());
}
