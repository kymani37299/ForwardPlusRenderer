#include "GUI_Implementations.h"

#include "Core/SceneGraph.h"
#include "Gui/Imgui/imgui.h"
#include "Render/Resource.h"
#include "System/ApplicationConfiguration.h"

DebugToolsConfiguration DebugToolsConfig;
PostprocessingSettings PostprocessSettings;
RenderStatistics RenderStats;

// --------------------------------------------------
void DebugToolsGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Text("Geometry culling");
	ImGui::Checkbox("Disable geometry culling", &DebugToolsConfig.DisableGeometryCulling);
	ImGui::Checkbox("Freeze geometry culling", &DebugToolsConfig.FreezeGeometryCulling);
	ImGui::Checkbox("Draw bounding boxes", &DebugToolsConfig.DrawBoundingSpheres);
	ImGui::Separator();
	ImGui::Text("Light culling");
	ImGui::Checkbox("Freeze light culling", &DebugToolsConfig.FreezeLightCulling);
	ImGui::Checkbox("Disable light culling", &DebugToolsConfig.DisableLightCulling);
	ImGui::Checkbox("Light heatmap", &DebugToolsConfig.LightHeatmap);
	ImGui::Checkbox("Draw light spheres", &DebugToolsConfig.DrawLightSpheres);
	ImGui::Separator();
	ImGui::Checkbox("Use PBR", &DebugToolsConfig.UsePBR);
	
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
	Camera::CameraTransform t = MainSceneGraph.MainCamera.CurrentTranform;
	
	ImGui::Text("Position: (%.2f, %.2f, %.2f)", t.Position.x, t.Position.y, t.Position.z);
	ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", t.Rotation.x, t.Rotation.y, t.Rotation.z);
	ImGui::Text("Forward: (%.2f, %.2f, %.2f)", t.Forward.x, t.Forward.y, t.Forward.z);
	ImGui::Text("Right: (%.2f, %.2f, %.2f)", t.Right.x, t.Right.y, t.Right.z);
	ImGui::Text("Up: (%.2f, %.2f, %.2f)", t.Up.x, t.Up.y, t.Up.z);
}

// --------------------------------------------------
std::string ToString(AntiAliasingMode aaMode)
{
	switch (aaMode)
	{
	case AntiAliasingMode::None: return "None";
	case AntiAliasingMode::TAA: return "TAA";
	case AntiAliasingMode::MSAA: return "MSAA";
	default: NOT_IMPLEMENTED;
	}
	return "";
}

void PostprocessingGUI::Render(ID3D11DeviceContext* context)
{
	if (ImGui::BeginCombo("Antialiasing mode", ToString(PostprocessSettings.AntialiasingMode).c_str()))
	{
		const uint32_t modeCount = EnumToInt(AntiAliasingMode::Count);
		for (uint32_t i = 0; i < modeCount; i++)
		{
			bool isSelected = false;
			AntiAliasingMode mode = IntToEnum<AntiAliasingMode>(i);
			ImGui::Selectable(ToString(mode).c_str(), &isSelected);
			if (isSelected)
			{
				AntiAliasingMode lastMode = PostprocessSettings.AntialiasingMode;
				PostprocessSettings.AntialiasingMode = mode;
				AppConfig.WindowSizeDirty = true; // Reload the render targets
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Checkbox("Exposure tonemapping", &PostprocessSettings.UseExposureTonemapping);
	if(PostprocessSettings.UseExposureTonemapping)
		ImGui::SliderFloat("Exposure", &PostprocessSettings.Exposure, 0.01f, 10.0f);
}

// --------------------------------------------------
void RenderStatsGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Text("Lights: \t\t %u / %u", RenderStats.VisibleLights, RenderStats.TotalLights);
	ImGui::Text("Drawables: \t\t %u / %u", RenderStats.VisibleDrawables, RenderStats.TotalDrawables);
}

void TextureVisualizerGUI::Render(ID3D11DeviceContext* context)
{
	int val = m_SelectedTexture;
	ImGui::InputInt("Texture ID", &val);
	m_SelectedTexture = MIN((uint32_t) val, GFX::Storage::TEXTURE_STORAGE_SIZE);

	ImGui::SliderFloat("Texture size", &m_ScaleFactor, 0.0f, 1.0f);

	TextureID texture{ m_SelectedTexture };
	if (texture.Valid())
	{
		const Texture& tex = GFX::Storage::GetTexture(texture);
		if (tex.SRV.Get())
		{
			ImGui::Image(tex.SRV.Get(), { tex.Width * m_ScaleFactor, tex.Height * m_ScaleFactor });
		}
	}
}
