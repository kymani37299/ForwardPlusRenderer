#include "ShadowRenderer.h"

#include <Engine/Render/Texture.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Context.h>
#include <Engine/System/ApplicationConfiguration.h>

#include "Globals.h"
#include "Renderers/Util/ConstantBuffer.h"
#include "Renderers/Util/VertexPipeline.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneGraph.h"

ShadowRenderer::ShadowRenderer()
{

}

ShadowRenderer::~ShadowRenderer()
{

}

void ShadowRenderer::Init(GraphicsContext& context)
{
	m_Shadowmap = ScopedRef<Texture>(GFX::CreateTexture(1024, 1024, RCF::DSV));
	m_ShadowmapShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/depth.hlsl"));
	m_ShadowmaskShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/shadowmask.hlsl"));
	m_HzbGenerator.Init(context);
	ReloadTextureResources(context);

	GFX::SetDebugName(m_Shadowmap.get(), "ShadowRenderer::Shadowmap");
}

Texture* ShadowRenderer::CalculateShadowMask(GraphicsContext& context, Texture* depth)
{
	PROFILE_SECTION(context, "Shadows");

	GFX::Cmd::ClearDepthStencil(context, m_Shadowmap.get());

	// Shadowmap
	{
		PROFILE_SECTION(context, "Shadowmap");

		GraphicsState state;
		ConstantBuffer cb{};
		cb.Add(SceneManager::Get().GetSceneGraph().ShadowCamera.CameraData);

		state.Table.SMPs[0] = { D3D12_FILTER_ANISOTROPIC , D3D12_TEXTURE_ADDRESS_MODE_WRAP };
		state.Table.CBVs[0] = cb.GetBuffer(context);
		state.DepthStencilState.DepthEnable = true;
		state.Shader = m_ShadowmapShader.get();

		RenderGroupType prepassTypes[] = { RenderGroupType::Opaque, RenderGroupType::AlphaDiscard };
		for (uint32_t i = 0; i < STATIC_ARRAY_SIZE(prepassTypes); i++)
		{
			RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
			RenderGroup& renderGroup = SceneManager::Get().GetSceneGraph().RenderGroups[i];

			std::vector<std::string> config{};
			config.push_back("SHADOWMAP");
			state.DepthStencil = m_Shadowmap.get();
			
			if (rgType == RenderGroupType::AlphaDiscard)
			{
				config.push_back("ALPHA_DISCARD");
			}
			state.ShaderConfig = config;
			VertPipeline->Draw(context, state, renderGroup, SceneManager::Get().GetSceneGraph().ShadowCamera.CullingData[rgType]);
		}
	}

	// Shadowmask
	{
		PROFILE_SECTION(context, "Shadowmask");

		GraphicsState state;

		ConstantBuffer cb{};
		cb.Add(SceneManager::Get().GetSceneGraph().MainCamera.CameraData);
		cb.Add(SceneManager::Get().GetSceneGraph().ShadowCamera.CameraData);
		cb.Add(SceneManager::Get().GetSceneGraph().SceneInfoData);

		state.Table.SMPs[0] = { D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_WRAP };
		state.Table.CBVs[0] = cb.GetBuffer(context);
		state.Table.SRVs[0] = depth;
		state.Table.SRVs[1] = m_Shadowmap.get();
		state.RenderTargets[0] = m_Shadowmask.get();
		state.Shader = m_ShadowmaskShader.get();

		GFX::Cmd::DrawFC(context, state);
	}

	return m_Shadowmask.get();
}

void ShadowRenderer::ReloadTextureResources(GraphicsContext& context)
{
	m_Shadowmask = ScopedRef<Texture>(GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF::RTV, 1, DXGI_FORMAT_R32_FLOAT));
	GFX::SetDebugName(m_Shadowmask.get(), "ShadowRenderer::Shadowmask");
}

Texture* ShadowRenderer::GetHZB(GraphicsContext& context)
{
	return m_HzbGenerator.GetHZB(context, m_Shadowmap.get(), SceneManager::Get().GetSceneGraph().ShadowCamera);
}
