#include "PostprocessingGUI.h"

#include "Gui/Imgui/imgui.h"

PostprocessingSettings PostprocessSettings;

void PostprocessingGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Checkbox("TAA", &PostprocessSettings.EnableTAA);
}