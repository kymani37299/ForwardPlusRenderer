#include "DebugToolsGUI.h"

#include "Gui/Imgui/imgui.h"

DebugToolsConfiguration DebugToolsConfig;

void DebugToolsGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Begin("Debug tools", &m_Shown);
	ImGui::Checkbox("Disable geometry culling", &DebugToolsConfig.DisableGeometryCulling);
	ImGui::Checkbox("Freeze geometry culling", &DebugToolsConfig.FreezeGeometryCulling);
	ImGui::Checkbox("Freeze light culling", &DebugToolsConfig.FreezeLightCulling);
	ImGui::Checkbox("Disable light culling", &DebugToolsConfig.DisableLightCulling);
	ImGui::End();
}