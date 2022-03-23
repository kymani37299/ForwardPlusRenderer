#include "FPSCounterGUI.h"

#include "Gui/Imgui/imgui.h"

void FPSCounterGUI::Update(float dt)
{
	m_CurrentDT = dt;
}

void FPSCounterGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Begin("FPS", &m_Shown);
	ImGui::Text("Frame: %.2f ms", m_CurrentDT);
	ImGui::End();
}