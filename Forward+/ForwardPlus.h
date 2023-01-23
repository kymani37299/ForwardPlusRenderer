#pragma once

#include <Engine/Common.h>
#include <Engine/Core/Application.h>

#include "Renderers/Culling.h"
#include "Renderers/DebugRenderer.h"
#include "Renderers/SkyboxRenderer.h"
#include "Renderers/PostprocessingRenderer.h"
#include "Renderers/ShadowRenderer.h"
#include "Renderers/GeometryRenderer.h"
#include "Renderers/SSAORenderer.h"

struct GraphicsContext;
struct Texture;
struct Buffer;
struct Shader;

class ForwardPlus : public Application
{
public:
	ForwardPlus();
	~ForwardPlus();

	void OnInit(GraphicsContext& context);
	void OnDestroy(GraphicsContext& context);

	Texture* OnDraw(GraphicsContext& context);
	void OnUpdate(GraphicsContext& context, float dt);

	void OnShaderReload(GraphicsContext& context);
	void OnWindowResize(GraphicsContext& context);

private:

	Culling m_Culling;
	DebugRenderer m_DebugRenderer;
	SkyboxRenderer m_SkyboxRenderer{ "Resources/skybox_panorama.hdr" };
	PostprocessingRenderer m_PostprocessingRenderer;
	ShadowRenderer m_ShadowRenderer;
	GeometryRenderer m_GeometryRenderer;
	SSAORenderer m_SSAORenderer;
	TextureDebuggerRenderer m_TextureDebuggerRenderer;

	ScopedRef<Shader> m_DepthResolveShader;

	// GFX Resources
	ScopedRef<Texture> m_MainRT_HDR;
	Ref<Texture> m_MainRT_DepthMS;
	Ref<Texture> m_MainRT_Depth;
	ScopedRef<Texture> m_MotionVectorRT;
};