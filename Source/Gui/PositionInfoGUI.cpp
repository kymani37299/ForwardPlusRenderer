#include "PositionInfoGUI.h"

#include "Core/SceneGraph.h"
#include "Gui/Imgui/imgui.h"

void PositionInfoGUI::Render(ID3D11DeviceContext* context)
{
	Float3 position = MainSceneGraph.MainCamera.CurrentTranform.Position;

	ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
}