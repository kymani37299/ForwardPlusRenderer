#pragma once

#include <Engine/Core/Application.h>
#include <Engine/Render/ResourceID.h>

#include "Renderers/Culling.h"
#include "Renderers/DebugRenderer.h"
#include "Renderers/SkyboxRenderer.h"
#include "Renderers/PostprocessingRenderer.h"
#include "Renderers/ShadowRenderer.h"
#include "Renderers/GeometryRenderer.h"
#include "Renderers/SSAORenderer.h"

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

	Culling m_Culling;
	DebugRenderer m_DebugRenderer;
	SkyboxRenderer m_SkyboxRenderer{ "Resources/skybox_panorama.hdr" };
	PostprocessingRenderer m_PostprocessingRenderer;
	ShadowRenderer m_ShadowRenderer;
	GeometryRenderer m_GeometryRenderer;
	SSAORenderer m_SSAORenderer;

	ShaderID m_DepthResolveShader;

	// GFX Resources
	TextureID m_MainRT_HDR;
	TextureID m_MainRT_DepthMS;
	TextureID m_MainRT_Depth;
	TextureID m_MotionVectorRT;
};