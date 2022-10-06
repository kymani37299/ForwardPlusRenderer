#include "ShadowRenderer.h"

#include <Engine/Render/Texture.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Context.h>
#include <Engine/System/ApplicationConfiguration.h>

#include "Globals.h"
#include "Renderers/Util/ConstantManager.h"
#include "Renderers/Util/SamplerManager.h"
#include "Renderers/Util/VertexPipeline.h"
#include "Scene/SceneGraph.h"

ShadowRenderer::ShadowRenderer()
{

}

ShadowRenderer::~ShadowRenderer()
{

}

void ShadowRenderer::Init(GraphicsContext& context)
{
	m_Shadowmap = ScopedRef<Texture>(GFX::CreateTexture(1024, 1024, RCF_Bind_DSV));
	m_ShadowmapShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/depth.hlsl"));
	m_ShadowmaskShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/shadowmask.hlsl"));
	ReloadTextureResources(context);

	GFX::SetDebugName(m_Shadowmap.get(), "ShadowRenderer::Shadowmap");
}

Texture* ShadowRenderer::CalculateShadowMask(GraphicsContext& context, Texture* depth)
{
	GFX::Cmd::MarkerBegin(context, "Shadows");

	GFX::Cmd::ClearDepthStencil(context, m_Shadowmap.get());

	// Shadowmap
	{
		GFX::Cmd::MarkerBegin(context, "Shadowmap");

		GraphicsState state;
		CBManager.Clear();
		CBManager.Add(MainSceneGraph->ShadowCamera.CameraData);

		state.Table.CBVs.push_back(CBManager.GetBuffer());
		state.DepthStencilState.DepthEnable = true;
		state.Shader = m_ShadowmapShader.get();

		RenderGroupType prepassTypes[] = { RenderGroupType::Opaque, RenderGroupType::AlphaDiscard };
		for (uint32_t i = 0; i < STATIC_ARRAY_SIZE(prepassTypes); i++)
		{
			RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
			RenderGroup& renderGroup = MainSceneGraph->RenderGroups[i];

			std::vector<std::string> config{};
			config.push_back("SHADOWMAP");
			state.DepthStencil = m_Shadowmap.get();
			
			if (rgType == RenderGroupType::AlphaDiscard)
			{
				config.push_back("ALPHA_DISCARD");
				SSManager.Bind(state);
			}
			state.ShaderConfig = config;
			VertPipeline->Draw(context, state, renderGroup, true);
		}
		GFX::Cmd::MarkerEnd(context);
	}

	// Shadowmask
	{
		GraphicsState state;

		CBManager.Clear();
		CBManager.Add(MainSceneGraph->MainCamera.CameraData);
		CBManager.Add(MainSceneGraph->ShadowCamera.CameraData);
		CBManager.Add(MainSceneGraph->SceneInfoData);
		state.Table.CBVs.push_back(CBManager.GetBuffer());
		state.Table.SRVs.resize(2);
		state.Table.SRVs[0] = depth;
		state.Table.SRVs[1] = m_Shadowmap.get();
		state.RenderTargets.push_back(m_Shadowmask.get());
		state.Shader = m_ShadowmaskShader.get();

		GFX::Cmd::MarkerBegin(context, "Shadowmask");
		SSManager.Bind(state);
		GFX::Cmd::DrawFC(context, state);
		GFX::Cmd::MarkerEnd(context);
	}

	GFX::Cmd::MarkerEnd(context);

	return m_Shadowmask.get();
}

void ShadowRenderer::ReloadTextureResources(GraphicsContext& context)
{
	m_Shadowmask = ScopedRef<Texture>(GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV, 1, DXGI_FORMAT_R32_FLOAT));
	GFX::SetDebugName(m_Shadowmask.get(), "ShadowRenderer::Shadowmask");
}
