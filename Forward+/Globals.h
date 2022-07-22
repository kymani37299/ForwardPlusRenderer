#pragma once

struct DebugToolsConfiguration
{
	bool DisableGeometryCulling = false;
	bool FreezeGeometryCulling = false;
	bool DrawBoundingSpheres = false;
	bool UseMeshletCulling = true;

	bool DisableLightCulling = false;
	bool FreezeLightCulling = false;
	bool LightHeatmap = false;
	bool DrawLightSpheres = false;

	bool UsePBR = true;
	bool UseIBL = true;
};

enum class AntiAliasingMode
{
	None,
	TAA,
	MSAA,
	Count,
};

struct BloomSettings
{
	bool Enabled = true;
	float FTheshold = 1.0f;
	float FKnee = 0.4f;
	Float4 SamplingScale{ 1.0f, 1.0f, 1.0f, 1.0f };
};

struct SSAOSettings
{
	bool Enabled = true;
	float DepthBias = 0.0001f;
	float SampleRadius = 0.5f;
	float Power = 1.5f;
};

struct RendererSettings
{
	AntiAliasingMode AntialiasingMode = AntiAliasingMode::MSAA;
	float Exposure = 1.0f;
	BloomSettings Bloom;
	SSAOSettings SSAO;
};

struct RenderStatistics
{
	uint32_t TotalDrawables;
	uint32_t VisibleDrawables;
};

extern RenderStatistics RenderStats;
extern RendererSettings RenderSettings;
extern DebugToolsConfiguration DebugToolsConfig;