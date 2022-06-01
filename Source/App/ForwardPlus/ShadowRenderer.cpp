#include "ShadowRenderer.h"

#include "App/ForwardPlus/VertexPipeline.h"
#include "Core/SceneGraph.h"
#include "Render/Texture.h"
#include "Render/Commands.h"
#include "Gui/GUI_Implementations.h"
#include "System/ApplicationConfiguration.h"

void ShadowRenderer::Init(ID3D11DeviceContext* context)
{
	m_Shadowmap = GFX::CreateTexture(1024, 1024, RCF_Bind_DSV | RCF_Bind_SRV);
	m_ShadowmapShader = GFX::CreateShader("Source/Shaders/depth.hlsl");
	m_ShadowmaskShader = GFX::CreateShader("Source/Shaders/shadowmask.hlsl");
	ReloadTextureResources(context);
}

TextureID ShadowRenderer::CalculateShadowMask(ID3D11DeviceContext* context, TextureID depth)
{
	GFX::Cmd::ClearDepthStencil(context, m_Shadowmap);

	// Shadowmap
	{
		GFX::Cmd::MarkerBegin(context, "Shadowmap");
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;

		RenderGroupType prepassTypes[] = { RenderGroupType::Opaque, RenderGroupType::AlphaDiscard };
		for (uint32_t i = 0; i < STATIC_ARRAY_SIZE(prepassTypes); i++)
		{
			RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
			RenderGroup& renderGroup = MainSceneGraph.RenderGroups[i];

			std::vector<std::string> config{};

			GFX::Cmd::BindRenderTarget(context, TextureID{}, m_Shadowmap);
			GFX::Cmd::SetPipelineState(context, pso);
			GFX::Cmd::BindCBV<VS | PS>(context, MainSceneGraph.ShadowCamera.CameraBuffer, 0);
			if (rgType == RenderGroupType::AlphaDiscard)
			{
				config.push_back("ALPHA_DISCARD");
				GFX::Cmd::SetupStaticSamplers<PS>(context);
			}

			GFX::Cmd::BindShader<VS | PS>(context, m_ShadowmapShader, config, true);
			VertPipeline.Draw(context, renderGroup, true);
		}
		GFX::Cmd::MarkerEnd(context);
	}

	// Shadowmask
	{
		std::vector<std::string> config;
		if (PostprocessSettings.AntialiasingMode == AntiAliasingMode::MSAA) config.push_back("MULTISAMPLE_DEPTH");

		GFX::Cmd::MarkerBegin(context, "Shadowmask");
		GFX::Cmd::SetupStaticSamplers<PS>(context);
		GFX::Cmd::BindRenderTarget(context, m_Shadowmask);
		GFX::Cmd::BindSRV<PS>(context, depth, 0);
		GFX::Cmd::BindSRV<PS>(context, m_Shadowmap, 1);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.ShadowCamera.CameraBuffer, 1);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.SceneInfoBuffer, 2);
		GFX::Cmd::BindShader<VS | PS>(context, m_ShadowmaskShader, config);
		GFX::Cmd::DrawFC(context);
		GFX::Cmd::BindSRV<PS>(context, nullptr, 0);
		GFX::Cmd::MarkerEnd(context);
	}

	return m_Shadowmask;
}

void ShadowRenderer::ReloadTextureResources(ID3D11DeviceContext* context)
{
	if (m_Shadowmask.Valid()) GFX::Storage::Free(m_Shadowmask);
	m_Shadowmask = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R32_FLOAT);
}
