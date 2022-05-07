#include "DebugToolsGUI.h"

#include "Gui/Imgui/imgui.h"

DebugToolsConfiguration DebugToolsConfig;

void DebugToolsGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Begin("Debug tools", &m_Shown);
	ImGui::Checkbox("Freeze culling", &DebugToolsConfig.FreezeCulling);
	ImGui::End();
}