#pragma once

#include <Engine/Core/Application.h>
#include <Engine/Render/ResourceID.h>

#include "Renderers/DebugRenderer.h"
#include "Renderers/SkyboxRenderer.h"
#include "Renderers/PostprocessingRenderer.h"
#include "Renderers/ShadowRenderer.h"

class ForwardPlus : public Application
{
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

private:
	uint32_t m_NumTilesX = 0;
	uint32_t m_NumTilesY = 0;

	DebugRenderer m_DebugRenderer;
	SkyboxRenderer m_SkyboxRenderer{ "Resources/skybox_panorama.hdr" };
	PostprocessingRenderer m_PostprocessingRenderer;
	ShadowRenderer m_ShadowRenderer;

	// GFX Resources
	ShaderID m_DepthPrepassShader;
	ShaderID m_GeometryShader;
	ShaderID m_LightCullingShader;

	TextureID m_MainRT_HDR;
	TextureID m_MainRT_Depth;
	TextureID m_MotionVectorRT;

	BufferID m_VisibleLightsBuffer;
};