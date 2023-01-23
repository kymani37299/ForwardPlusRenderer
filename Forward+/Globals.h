#pragma once

struct DebugVisualizations
{
	bool BoundingSpheres = false;
	bool LightHeatmap = false;
	bool LightSpheres = false;
};

enum class AntiAliasingMode
{
	None,
	TAA,
	MSAA,
	Count,
};

enum class GeometryCullingMode
{
	None,
	CPU_FrustumCulling,
	GPU_FrustumCulling,
	GPU_OcclusionCulling,
	Count
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
	bool Enabled = false;
	float DepthBias = 0.0001f;
	float SampleRadius = 0.5f;
	float Power = 1.5f;
};

struct CullingSettings
{
	bool LightCullingEnabled = true;
	
	bool GeometryCullingFrozen = false;
	GeometryCullingMode GeoCullingMode = GeometryCullingMode::GPU_OcclusionCulling;
};

struct ShadingSettings
{
	bool UsePBR = true;
	bool UseIBL = true;
};

struct RendererSettings
{
	AntiAliasingMode AntialiasingMode = AntiAliasingMode::MSAA;
	float Exposure = 1.0f;
	BloomSettings Bloom;
	SSAOSettings SSAO;
	CullingSettings Culling;
	ShadingSettings Shading;
};

struct CullingStatistics
{
	uint32_t TotalDrawables;
	uint32_t VisibleDrawables;

	uint32_t TotalTriangles;
	uint32_t VisibleTriangles;
};

struct RenderStatistics
{
	CullingStatistics MainStats;
	CullingStatistics ShadowStats;
};

extern RenderStatistics RenderStats;
extern RendererSettings RenderSettings;
extern DebugVisualizations DebugViz;