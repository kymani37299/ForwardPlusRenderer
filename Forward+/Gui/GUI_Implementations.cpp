#include "GUI_Implementations.h"

#include <Engine/Gui/Imgui/imgui.h>
#include <Engine/Render/Resource.h>
#include <Engine/System/ApplicationConfiguration.h>
#include <Engine/Utility/Random.h>

#include "Scene/SceneGraph.h"

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
	ImGui::Checkbox("Use IBL", &DebugToolsConfig.UseIBL);
	
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

void RenderSettingsGUI::Render(ID3D11DeviceContext* context)
{
	if (ImGui::BeginCombo("Antialiasing mode", ToString(RenderSettings.AntialiasingMode).c_str()))
	{
		const uint32_t modeCount = EnumToInt(AntiAliasingMode::Count);
		for (uint32_t i = 0; i < modeCount; i++)
		{
			bool isSelected = false;
			AntiAliasingMode mode = IntToEnum<AntiAliasingMode>(i);
			ImGui::Selectable(ToString(mode).c_str(), &isSelected);
			if (isSelected)
			{
				AntiAliasingMode lastMode = RenderSettings.AntialiasingMode;
				RenderSettings.AntialiasingMode = mode;
				AppConfig.WindowSizeDirty = true; // Reload the render targets
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SliderFloat("Exposure", &RenderSettings.Exposure, 0.01f, 10.0f);

	if (ImGui::CollapsingHeader("Bloom"))
	{
		ImGui::Checkbox("Enabled", &RenderSettings.Bloom.Enabled);
		if (RenderSettings.Bloom.Enabled)
		{
			ImGui::SliderFloat("Treshold", &RenderSettings.Bloom.FTheshold, 0.0f, 5.0f);
			ImGui::SliderFloat("Knee", &RenderSettings.Bloom.FKnee, 0.0f, 5.0f);

			float sampleScale[4];
			sampleScale[0] = RenderSettings.Bloom.SamplingScale.x;
			sampleScale[1] = RenderSettings.Bloom.SamplingScale.y;
			sampleScale[2] = RenderSettings.Bloom.SamplingScale.z;
			sampleScale[3] = RenderSettings.Bloom.SamplingScale.w;
			ImGui::SliderFloat4("Sample scale", sampleScale, 0.0f, 5.0f);
			RenderSettings.Bloom.SamplingScale.x = sampleScale[0];
			RenderSettings.Bloom.SamplingScale.y = sampleScale[1];
			RenderSettings.Bloom.SamplingScale.z = sampleScale[2];
			RenderSettings.Bloom.SamplingScale.w = sampleScale[3];
		}
	}

	if (ImGui::CollapsingHeader("SSAO"))
	{
		ImGui::Checkbox(" Enabled", &RenderSettings.SSAO.Enabled); // TODO: Checkboxes cant have same name, fix this somehow
		if (RenderSettings.SSAO.Enabled)
		{
			ImGui::SliderFloat("Radius", &RenderSettings.SSAO.SampleRadius, 0.0f, 2.0f);
			ImGui::SliderFloat("Power", &RenderSettings.SSAO.Power, 0.0f, 4.0f);
			ImGui::SliderFloat("Depth bias", &RenderSettings.SSAO.DepthBias, 0.001f, 0.25f);
		}
	}
}

void RenderStatsGUI::Update(float dt)
{
	static constexpr float UpdateInterval = 200.0f; // In ms
	static float lastUpdate = 0.0f;

	lastUpdate += dt;
	DTHistory.push_back(dt);
	if (lastUpdate > UpdateInterval)
	{
		uint32_t count = DTHistory.size();
		float sum = 0.0f;
		for (float _dt : DTHistory)
		{
			sum += _dt;
		}

		lastUpdate -= UpdateInterval;
		m_CurrentDT = sum / count;

		DTHistory.clear();
	}
}

// --------------------------------------------------
void RenderStatsGUI::Render(ID3D11DeviceContext* context)
{
	ImGui::Text("Frame: %.2f ms", m_CurrentDT);
	ImGui::Text("FPS:   %u", static_cast<uint32_t>(1000.0f / m_CurrentDT));
	ImGui::Separator();
	ImGui::Text("Num lights:  %u", MainSceneGraph.Lights.GetSize());
	ImGui::Text("Drawables:   %u / %u", RenderStats.VisibleDrawables, RenderStats.TotalDrawables);
}

// --------------------------------------------------
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

// --------------------------------------------------
void LightsGUI::Render(ID3D11DeviceContext* context)
{
	if (ImGui::Button("Generate 1k lights"))
	{
		for (uint32_t i = 0; i < 1000; i++)
		{
			const float strength = Random::Float(1.0f, 5.0f);
			const Float3 position = Float3(1000.0f, 80.0f, 1000.0f) * Float3(Random::UNorm(), Random::UNorm(), Random::UNorm());
			const Float3 color = strength * Float3(Random::UNorm(), Random::UNorm(), Random::UNorm());
			const Float2 falloff = strength * Float2(0.5f + 2.0f * Random::UNorm(), 3.0f + 2.0f * Random::UNorm());
			MainSceneGraph.CreatePointLight(context, position, color, falloff);
		}
	}

	ImGui::Separator();

	if (MainSceneGraph.DirLightIndex != UINT32_MAX)
	{
		Light& dirLight = MainSceneGraph.Lights[MainSceneGraph.DirLightIndex];

		float direction[3];
		direction[0] = dirLight.Direction.x;
		direction[1] = dirLight.Direction.y;
		direction[2] = dirLight.Direction.z;

		ImGui::Text("Directional light");
		bool changed = ImGui::SliderFloat3("Direction", direction, -1.0f, 1.0f);
		dirLight.Direction = { direction[0], direction[1], direction[2] };
		dirLight.Direction = dirLight.Direction.Normalize();

		float color[3];
		color[0] = dirLight.Radiance.x;
		color[1] = dirLight.Radiance.y;
		color[2] = dirLight.Radiance.z;
		changed = ImGui::SliderFloat3("Color", color, 0.0f, 5.0f) || changed;

		dirLight.Radiance = { color[0], color[1], color[2] };

		if (changed)
		{
			dirLight.UpdateBuffer(context);
		}
	}
}