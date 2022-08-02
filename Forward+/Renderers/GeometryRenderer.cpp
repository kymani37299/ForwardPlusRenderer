#include "GeometryRenderer.h"

#include <Engine/Render/Context.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Texture.h>

#include "Globals.h"
#include "Renderers/Util/ConstantManager.h"
#include "Renderers/Util/SamplerManager.h"
#include "Renderers/Util/VertexPipeline.h"
#include "Scene/SceneGraph.h"

GeometryRenderer::GeometryRenderer()
{

}

GeometryRenderer::~GeometryRenderer()
{

}

void GeometryRenderer::Init(GraphicsContext& context)
{
	m_DepthPrepassShader = ScopedRef<Shader>(new Shader{ "Forward+/Shaders/depth.hlsl" });
	m_GeometryShader = ScopedRef<Shader>(new Shader{ "Forward+/Shaders/geometry.hlsl" });
}

void GeometryRenderer::DepthPrepass(GraphicsContext& context, GraphicsState& state)
{
	const bool drawMotionVectors = RenderSettings.AntialiasingMode == AntiAliasingMode::TAA;

	GFX::Cmd::MarkerBegin(context, "Depth Prepass");

	if(state.Table.CBVs.size() < 1) state.Table.CBVs.resize(1);

	CBManager.Clear();
	CBManager.Add(MainSceneGraph->MainCamera.CameraData);
	CBManager.Add(MainSceneGraph->MainCamera.LastCameraData);
	CBManager.Add(MainSceneGraph->SceneInfoData);
	state.Table.CBVs[0] = CBManager.GetBuffer();
	state.Pipeline.DepthStencilState.DepthEnable = true;

	RenderGroupType prepassTypes[] = { RenderGroupType::Opaque, RenderGroupType::AlphaDiscard };
	for (uint32_t i = 0; i < STATIC_ARRAY_SIZE(prepassTypes); i++)
	{
		RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
		RenderGroup& renderGroup = MainSceneGraph->RenderGroups[i];

		std::vector<std::string> config{};
		if (drawMotionVectors) config.push_back("MOTION_VECTORS");

		if (rgType == RenderGroupType::AlphaDiscard)
		{
			config.push_back("ALPHA_DISCARD");
			SSManager.Bind(state);
		}
		GFX::Cmd::BindShader(state, m_DepthPrepassShader.get(), VS | PS, config);
		VertPipeline->Draw(context, state, renderGroup);
	}
	GFX::Cmd::MarkerEnd(context);
}

void GeometryRenderer::Draw(GraphicsContext& context, GraphicsState& state, Texture* shadowMask, Buffer* visibleLights, Texture* irradianceMap, Texture* ambientOcclusion)
{
	GFX::Cmd::MarkerBegin(context, "Geometry");

	if (state.Table.CBVs.size() < 1) state.Table.CBVs.resize(1);

	D3D12_BLEND_DESC blendStateOff = state.Pipeline.BlendState;
	D3D12_BLEND_DESC blendStateOn = state.Pipeline.BlendState;
	blendStateOn.RenderTarget[0].BlendEnable = true;
	blendStateOn.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendStateOn.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
	blendStateOn.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendStateOn.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendStateOn.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	blendStateOn.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;

	CBManager.Clear();
	CBManager.Add(MainSceneGraph->MainCamera.CameraData);
	CBManager.Add(MainSceneGraph->SceneInfoData);
	state.Table.CBVs[0] = CBManager.GetBuffer();

	state.StencilRef = 0xff;
	state.Pipeline.DepthStencilState.DepthEnable = true;
	state.Pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	state.Pipeline.DepthStencilState.StencilEnable = true;
	state.Pipeline.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	state.Pipeline.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	state.Pipeline.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	state.Pipeline.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	state.Pipeline.DepthStencilState.BackFace = state.Pipeline.DepthStencilState.FrontFace;
	
	SSManager.Bind(state);

	if (state.Table.SRVs.size() < 5) state.Table.SRVs.resize(5);
	state.Table.SRVs[0] = MainSceneGraph->Lights.GetBuffer();
	state.Table.SRVs[1] = visibleLights;
	state.Table.SRVs[2] = shadowMask;
	state.Table.SRVs[3] = irradianceMap;
	state.Table.SRVs[4] = ambientOcclusion;

	for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
	{
		RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
		RenderGroup& renderGroup = MainSceneGraph->RenderGroups[i];
		if (renderGroup.Drawables.GetSize() == 0) continue;

		state.Pipeline.BlendState = rgType == RenderGroupType::Transparent ? blendStateOn : blendStateOff;

		std::vector<std::string> configuration;
		if (rgType == RenderGroupType::AlphaDiscard) configuration.push_back("ALPHA_DISCARD");
		if (rgType == RenderGroupType::Transparent) configuration.push_back("ALPHA_BLEND");
		if (!RenderSettings.Culling.LightCullingEnabled) configuration.push_back("DISABLE_LIGHT_CULLING");
		if (RenderSettings.Shading.UsePBR) configuration.push_back("USE_PBR");
		if (RenderSettings.Shading.UsePBR && RenderSettings.Shading.UseIBL) configuration.push_back("USE_IBL");
		
		GFX::Cmd::BindShader(state, m_GeometryShader.get(), VS | PS, configuration);
		VertPipeline->Draw(context, state, renderGroup);
	}

	GFX::Cmd::MarkerEnd(context);
}
