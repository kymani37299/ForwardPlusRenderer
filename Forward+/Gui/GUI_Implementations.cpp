#include "GUI_Implementations.h"

#include <Engine/Gui/Imgui_Core.h>
#include <Engine/Render/Resource.h>
#include <Engine/Render/Texture.h>
#include <Engine/System/ApplicationConfiguration.h>
#include <Engine/Utility/Random.h>
#include <Engine/Utility/StringUtility.h>

#include "Renderers/Util/TextureDebugger.h"
#include "Scene/SceneGraph.h"

// --------------------------------------------------
void DebugVisualizationsGUI::Render()
{
	ImGui::Checkbox("Bounding spheres", &DebugViz.BoundingSpheres);
	ImGui::Checkbox("Light spheres", &DebugViz.LightSpheres);
	ImGui::Checkbox("Light heatmap", &DebugViz.LightHeatmap);
	
}

// --------------------------------------------------
void PositionInfoGUI::Render()
{
	Camera::CameraTransform t = MainSceneGraph->MainCamera.CurrentTranform;
	
	ImGui::Text("Position: (%.2f, %.2f, %.2f)", t.Position.x, t.Position.y, t.Position.z);
	ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", t.Rotation.x, t.Rotation.y, t.Rotation.z);
	ImGui::Text("Forward: (%.2f, %.2f, %.2f)", t.Forward.x, t.Forward.y, t.Forward.z);
	ImGui::Text("Right: (%.2f, %.2f, %.2f)", t.Right.x, t.Right.y, t.Right.z);
	ImGui::Text("Up: (%.2f, %.2f, %.2f)", t.Up.x, t.Up.y, t.Up.z);
}

// --------------------------------------------------
static std::string ToString(AntiAliasingMode aaMode)
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

static std::string ToString(GeometryCullingMode gcMode)
{
	switch (gcMode)
	{
	case GeometryCullingMode::None: return "None";
	case GeometryCullingMode::CPU_FrustumCulling: return "CPU_FrustumCulling";
	case GeometryCullingMode::GPU_FrustumCulling: return "GPU_FrustumCulling";
	case GeometryCullingMode::GPU_OcclusionCulling: return "GPU_OcclusionCulling";
	default: NOT_IMPLEMENTED;
	}
	return "";
}

void RenderSettingsGUI::Render()
{
	if (ImGui::BeginCombo("Antialiasing mode", ToString(RenderSettings.AntialiasingMode).c_str()))
	{
		const uint32_t modeCount = EnumToInt(AntiAliasingMode::Count);
		for (uint32_t i = 0; i < modeCount; i++)
		{
			bool isSelected = false;
			const AntiAliasingMode mode = IntToEnum<AntiAliasingMode>(i);
			ImGui::Selectable(ToString(mode).c_str(), &isSelected);
			if (isSelected)
			{
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
			ImGui::InputFloat("Depth bias", &RenderSettings.SSAO.DepthBias);
		}
	}

	if (ImGui::CollapsingHeader("Culling"))
	{
		ImGui::Checkbox("Light culling", &RenderSettings.Culling.LightCullingEnabled);
		ImGui::Separator();

		ImGui::Checkbox("Freeze geometry culling", &RenderSettings.Culling.GeometryCullingFrozen);
		if (ImGui::BeginCombo("Geometry culling mode", ToString(RenderSettings.Culling.GeoCullingMode).c_str()))
		{
			const uint32_t modeCount = EnumToInt(GeometryCullingMode::Count);
			for (uint32_t i = 0; i < modeCount; i++)
			{
				bool isSelected = false;
				const GeometryCullingMode mode = IntToEnum<GeometryCullingMode>(i);
				ImGui::Selectable(ToString(mode).c_str(), &isSelected);
				if (isSelected)
				{
					RenderSettings.Culling.GeoCullingMode = mode;
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	if (ImGui::CollapsingHeader("Shading"))
	{
		ImGui::Checkbox("PBR", &RenderSettings.Shading.UsePBR);
		if (RenderSettings.Shading.UsePBR)
		{
			ImGui::Checkbox("IBL", &RenderSettings.Shading.UseIBL);
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
		size_t count = DTHistory.size();
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
void RenderStatsGUI::Render()
{
	ImGui::Text("Frame: %.2f ms", m_CurrentDT);
	ImGui::Text("FPS:   %u", static_cast<uint32_t>(1000.0f / m_CurrentDT));
	ImGui::Separator();
	ImGui::Text("Num lights:  %u", MainSceneGraph->Lights.GetSize());
	ImGui::Separator();
	ImGui::Text("Drawables(Main)  :   %u / %u", RenderStats.MainStats.VisibleDrawables, RenderStats.MainStats.TotalDrawables);
	ImGui::Text("Triangles(Main)  :   %s / %s", StringUtility::RepresentNumberWithSeparator(RenderStats.MainStats.VisibleTriangles, ' ').c_str(), StringUtility::RepresentNumberWithSeparator(RenderStats.MainStats.TotalTriangles, ' ').c_str());
	ImGui::Separator();
	ImGui::Text("Drawables(Shadow):   %u / %u", RenderStats.ShadowStats.VisibleDrawables, RenderStats.ShadowStats.TotalDrawables);
	ImGui::Text("Triangles(Shadow):   %s / %s", StringUtility::RepresentNumberWithSeparator(RenderStats.ShadowStats.VisibleTriangles, ' ').c_str(), StringUtility::RepresentNumberWithSeparator(RenderStats.ShadowStats.TotalTriangles, ' ').c_str());
}

// --------------------------------------------------
void LightsGUI::Render()
{
	uint32_t lightsToGenerate = 0;

	if (ImGui::Button("Generate 1k lights"))
	{
		lightsToGenerate = 1000;
	}

	if (ImGui::Button("Generate 10k lights"))
	{
		lightsToGenerate = 10000;
	}

	for (uint32_t i = 0; i < lightsToGenerate; i++)
	{
		const float strength = Random::Float(1.0f, 5.0f);
		const Float3 position = Float3(1000.0f, 80.0f, 1000.0f) * Float3(Random::UNorm(), Random::UNorm(), Random::UNorm());
		const Float3 color = strength * Float3(Random::UNorm(), Random::UNorm(), Random::UNorm());
		const Float2 falloff = strength * Float2(0.5f + 2.0f * Random::UNorm(), 3.0f + 2.0f * Random::UNorm());
		MainSceneGraph->CreatePointLight(Device::Get()->GetContext(), position, color, falloff);
	}

	ImGui::Separator();

	DirectionalLight& dirLight = MainSceneGraph->DirLight;

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
}

// --------------------------------------------------
void TextureDebuggerGUI::Update(float dt)
{
	TexDebugger.GetEnabledRef() = GetShownRef();
}

void TextureDebuggerGUI::Render()
{
	const auto& debugTextures = TexDebugger.GetTextures();
	
	if (debugTextures.empty()) 
		return;

	auto& selectedTexture = TexDebugger.GetSelectedTexture();
	if (ImGui::BeginCombo("Select texture", selectedTexture.Name.c_str()))
	{
		const uint32_t textureCount = (uint32_t) debugTextures.size();
		for (uint32_t i = 0; i < textureCount; i++)
		{
			bool isSelected = false;
			ImGui::Selectable(debugTextures[i].Name.c_str(), &isSelected);
			if (isSelected)
			{
				TexDebugger.SetSelectedTexture(i);
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Checkbox("Show alpha", &selectedTexture.ShowAlpha);
	ImGui::SliderInt("Selected mip", &selectedTexture.SelectedMip, 0, selectedTexture.Tex->NumMips - 1);
	
	ImGui::SliderFloat("Range min", &selectedTexture.RangeMin, 0.0f, 1.0f);
	ImGui::SliderFloat("Range max", &selectedTexture.RangeMax, 0.0f, 1.0f);

	if (ImGui::Button("Find range"))
	{
		selectedTexture.FindRange = true;
	}

	if (selectedTexture.RangeMin > selectedTexture.RangeMax)
		selectedTexture.RangeMin = selectedTexture.RangeMax;

	if (TexDebugger.GetPreviewTextureHandle())
	{
		const float textureAspect = (float)selectedTexture.Tex->Height / selectedTexture.Tex->Width;

		static constexpr float previewSize = 512.0f;

		ImTextureID textureHandle = *((ImTextureID*)TexDebugger.GetPreviewTextureHandle());
		ImGui::Image(textureHandle, { previewSize, previewSize * textureAspect });
	}
}
