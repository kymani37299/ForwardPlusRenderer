#include "PositionInfoGUI.h"

#include "Core/SceneGraph.h"
#include "Gui/Imgui/imgui.h"

void PositionInfoGUI::Render(ID3D11DeviceContext* context)
{
	Float3 position = MainSceneGraph.MainCamera.Position;

	ImGui::Begin("Posistion info", &m_Shown);
	ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
	ImGui::End();
}