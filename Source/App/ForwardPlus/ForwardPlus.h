#pragma once

#include "App/Application.h"
#include "App/ForwardPlus/DebugRenderer.h"
#include "App/ForwardPlus/SkyboxRenderer.h"

#include "Render/ResourceID.h"

class ForwardPlus : public Application
{
	// Note: Duplicate definitions (see also light_culling.h)
	static constexpr uint8_t TILE_SIZE = 16;
	static constexpr uint32_t MAX_LIGHTS_PER_TILE = 1024;
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
	void UpdatePostprocessingSettings(ID3D11DeviceContext* context);

private:
	uint32_t m_NumTilesX = 0;
	uint32_t m_NumTilesY = 0;

	DebugRenderer m_DebugRenderer;
	SkyboxRenderer m_SkyboxRenderer{ "Resources/skybox_panorama.hdr" };

	// GFX Resources
	ShaderID m_DepthPrepassShader;
	ShaderID m_GeometryShader;
	ShaderID m_LightCullingShader;
	ShaderID m_PostprocessShader;

	TextureID m_TAAHistory[2]; // 0 - current frame, 1 - last frame

	TextureID m_MainRT_HDR;
	TextureID m_MainRT_LDR;
	TextureID m_MainRT_Depth;
	TextureID m_MotionVectorRT;

	BufferID m_IndexBuffer;
	BufferID m_VisibleLightsBuffer;
	BufferID m_PostprocessingSettingsBuffer;
};