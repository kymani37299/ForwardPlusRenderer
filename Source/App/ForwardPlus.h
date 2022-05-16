#pragma once

#include "App/Application.h"
#include "Render/ResourceID.h"

struct DebugGeometry
{
	Float3 Position;
	float Scale;
	Float4 Color;
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

private:
	BitField m_VisibilityMask;
	uint32_t m_NumTilesX = 0;
	uint32_t m_NumTilesY = 0;
	std::vector<DebugGeometry> m_DebugGeometries;

	// GFX Resources
	ShaderID m_SkyboxShader;
	ShaderID m_ShadowmapShader;
	ShaderID m_DepthPrepassShader;
	ShaderID m_GeometryShader;
	ShaderID m_GeometryShaderNoLightCulling;
	ShaderID m_GeometryAlphaDiscardShader;
	ShaderID m_GeometryAlphaDiscardShaderNoLightCulling;
	ShaderID m_LightCullingShader;
	ShaderID m_LightStatsShader;
	ShaderID m_DebugGeometryShader;
	ShaderID m_LightHeatmapShader;
	ShaderID m_TAA;

	TextureID m_SkyboxCubemap;
	TextureID m_FinalRT;
	TextureID m_MotionVectorRT;
	TextureID m_FinalRTSRV;
	TextureID m_FinalRTHistory[2];
	TextureID m_FinalRT_Depth;

	BufferID m_IndexBuffer;
	BufferID m_VisibleLightsBuffer;
	BufferID m_LightStatsBuffer;
	BufferID m_DebugGeometryBuffer;
};