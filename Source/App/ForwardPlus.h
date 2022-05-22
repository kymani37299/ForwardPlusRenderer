#pragma once

#include "App/Application.h"
#include "Render/ResourceID.h"

enum class DebugGeometryType
{
	CUBE,
	SPHERE,
	PLANE
};

struct DebugGeometry
{
	DebugGeometryType Type;

	// ALL
	Float3 Scale;
	Float4 Color;

	// CUBE, SPHERE
	Float3 Position; 

	// PLANE
	Float3 Normal; 
	float Distance;
};

class ForwardPlus : public Application
{
	// Note: Duplicate definitions (see also light_culling.h)
	static constexpr uint8_t TILE_SIZE = 16;
	static constexpr uint32_t MAX_LIGHTS_PER_TILE = 1024;

	static constexpr bool ENABLE_STATS = true;
public:
	void OnInit(ID3D11DeviceContext* context);
	void OnDestroy(ID3D11DeviceContext* context);

	TextureID OnDraw(ID3D11DeviceContext* context);
	void OnUpdate(ID3D11DeviceContext* context, float dt);

	void OnShaderReload(ID3D11DeviceContext* context);
	void OnWindowResize(ID3D11DeviceContext* context);

private:
	void UpdatePresentResources(ID3D11DeviceContext* context);
	void UpdateCullingResources(ID3D11DeviceContext* context);
	void UpdateStats(ID3D11DeviceContext* context);
	void DrawDebugGeometries(ID3D11DeviceContext* context);
	void UpdatePostprocessingSettings(ID3D11DeviceContext* context);

private:
	uint32_t m_NumTilesX = 0;
	uint32_t m_NumTilesY = 0;
	std::vector<DebugGeometry> m_DebugGeometries;

	// GFX Resources
	ShaderID m_SkyboxShader;
	ShaderID m_ShadowmapShader;
	ShaderID m_DepthPrepassShader;
	ShaderID m_GeometryShader;
	ShaderID m_LightCullingShader;
	ShaderID m_LightStatsShader;
	ShaderID m_DebugGeometryShader;
	ShaderID m_LightHeatmapShader;
	ShaderID m_PostprocessShader;

	TextureID m_SkyboxCubemap;
	TextureID m_TAAHistory[2]; // 0 - current frame, 1 - last frame

	TextureID m_MainRT_HDR;
	TextureID m_MainRT_LDR;
	TextureID m_MainRT_Depth;
	TextureID m_MotionVectorRT;

	BufferID m_IndexBuffer;
	BufferID m_VisibleLightsBuffer;
	BufferID m_LightStatsBuffer;
	BufferID m_DebugGeometryBuffer;
	BufferID m_PostprocessingSettingsBuffer;
};