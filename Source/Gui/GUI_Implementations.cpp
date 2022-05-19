#include "GUI_Implementations.h"

#include "Core/SceneGraph.h"
#include "Gui/Imgui/imgui.h"

DebugToolsConfiguration DebugToolsConfig;
PostprocessingSettings PostprocessSettings;
RenderStatistics RenderStats;

// --------------------------------------------------
void DebugToolsGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Checkbox("Disable geometry culling", &DebugToolsConfig.DisableGeometryCulling);
	ImGui::Checkbox("Freeze geometry culling", &DebugToolsConfig.FreezeGeometryCulling);
	ImGui::Checkbox("Freeze light culling", &DebugToolsConfig.FreezeLightCulling);
	ImGui::Checkbox("Disable light culling", &DebugToolsConfig.DisableLightCulling);
	ImGui::Checkbox("Draw bounding boxes", &DebugToolsConfig.DrawBoundingBoxes);
	ImGui::Checkbox("Light heatmap", &DebugToolsConfig.LightHeatmap);
}

// --------------------------------------------------
void FPSCounterGUI::Update(float dt)
{
	m_CurrentDT = dt;
}

void FPSCounterGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Text("Frame: %.2f ms", m_CurrentDT);
}

// --------------------------------------------------
void PositionInfoGUI::Render(ID3D11DeviceContext* context)
{
	Float3 position = MainSceneGraph.MainCamera.CurrentTranform.Position;

	ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
}

// --------------------------------------------------
void PostprocessingGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Checkbox("TAA", &PostprocessSettings.EnableTAA);
}

// --------------------------------------------------
void RenderStatsGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Text("Lights: \t\t %u / %u", RenderStats.VisibleLights, RenderStats.TotalLights);
	ImGui::Text("Drawables: \t\t %u / %u", RenderStats.VisibleDrawables, RenderStats.TotalDrawables);
}