#include "ShadowRenderer.h"

#include <Engine/Render/Texture.h>
#include <Engine/Render/Commands.h>
#include <Engine/System/ApplicationConfiguration.h>

#include "Globals.h"
#include "Renderers/Util/ConstantManager.h"
#include "Renderers/Util/SamplerManager.h"
#include "Renderers/Util/VertexPipeline.h"
#include "Scene/SceneGraph.h"

void ShadowRenderer::Init(ID3D11DeviceContext* context)
{
	m_Shadowmap = GFX::CreateTexture(1024, 1024, RCF_Bind_DSV | RCF_Bind_SRV);
	m_ShadowmapShader = GFX::CreateShader("Forward+/Shaders/depth.hlsl");
	m_ShadowmaskShader = GFX::CreateShader("Forward+/Shaders/shadowmask.hlsl");
	ReloadTextureResources(context);
}

TextureID ShadowRenderer::CalculateShadowMask(ID3D11DeviceContext* context, TextureID depth)
{
	GFX::Cmd::MarkerBegin(context, "Shadows");

	GFX::Cmd::ClearDepthStencil(context, m_Shadowmap);

	// Shadowmap
	{
		GFX::Cmd::MarkerBegin(context, "Shadowmap");
		
		CBManager.Clear();
		CBManager.Add(MainSceneGraph.ShadowCamera.CameraData, true);
		
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;

		RenderGroupType prepassTypes[] = { RenderGroupType::Opaque, RenderGroupType::AlphaDiscard };
		for (uint32_t i = 0; i < STATIC_ARRAY_SIZE(prepassTypes); i++)
		{
			RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
			RenderGroup& renderGroup = MainSceneGraph.RenderGroups[i];

			std::vector<std::string> config{};
			config.push_back("SHADOWMAP");

			GFX::Cmd::BindRenderTarget(context, TextureID{}, m_Shadowmap);
			GFX::Cmd::SetPipelineState(context, pso);
			if (rgType == RenderGroupType::AlphaDiscard)
			{
				config.push_back("ALPHA_DISCARD");
				SSManager.Bind(context);
			}

			GFX::Cmd::BindShader<VS | PS>(context, m_ShadowmapShader, config, true);
			VertPipeline.Draw(context, renderGroup, true);
		}
		GFX::Cmd::MarkerEnd(context);
	}

	// Shadowmask
	{
		CBManager.Clear();
		CBManager.Add(MainSceneGraph.MainCamera.CameraData);
		CBManager.Add(MainSceneGraph.ShadowCamera.CameraData);
		CBManager.Add(MainSceneGraph.SceneInfoData, true);

		GFX::Cmd::MarkerBegin(context, "Shadowmask");
		SSManager.Bind(context);
		GFX::Cmd::BindRenderTarget(context, m_Shadowmask);
		GFX::Cmd::BindSRV<PS>(context, depth, 0);
		GFX::Cmd::BindSRV<PS>(context, m_Shadowmap, 1);
		GFX::Cmd::BindShader<VS | PS>(context, m_ShadowmaskShader);
		GFX::Cmd::DrawFC(context);
		GFX::Cmd::BindSRV<PS>(context, nullptr, 0);
		GFX::Cmd::MarkerEnd(context);
	}

	GFX::Cmd::MarkerEnd(context);

	return m_Shadowmask;
}

void ShadowRenderer::ReloadTextureResources(ID3D11DeviceContext* context)
{
	if (m_Shadowmask.Valid()) GFX::Storage::Free(m_Shadowmask);
	m_Shadowmask = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R32_FLOAT);
}
