#include "RenderStatsGUI.h"

#include "Gui/Imgui/imgui.h"

RenderStatistics RenderStats;

void RenderStatsGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Text("Lights: \t\t %u / %u", RenderStats.VisibleLights, RenderStats.TotalLights);
	ImGui::Text("Drawables: \t\t %u / %u", RenderStats.VisibleDrawables, RenderStats.TotalDrawables);
}