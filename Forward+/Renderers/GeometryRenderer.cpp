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
		GFX::Cmd::BindShader(state, m_DepthPrepassShader.get(), VS | PS, config, true);
		VertPipeline->Draw(context, state, renderGroup);
	}
	GFX::Cmd::MarkerEnd(context);
}

void GeometryRenderer::Draw(GraphicsContext& context, GraphicsState& state, Texture* shadowMask, Buffer* visibleLights, Texture* irradianceMap, Texture* ambientOcclusion)
{
	GFX::Cmd::MarkerBegin(context, "Geometry");

	if (state.Table.CBVs.size() < 1) state.Table.CBVs.resize(1);

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
		RenderGroupType rgType = (RenderGroupType)i;
		RenderGroup& renderGroup = MainSceneGraph->RenderGroups[i];
		if (renderGroup.Drawables.GetSize() == 0) continue;

		std::vector<std::string> configuration;
		if (rgType == RenderGroupType::AlphaDiscard) configuration.push_back("ALPHA_DISCARD");
		if (DebugToolsConfig.DisableLightCulling) configuration.push_back("DISABLE_LIGHT_CULLING");
		if (DebugToolsConfig.UsePBR) configuration.push_back("USE_PBR");
		if (DebugToolsConfig.UsePBR && DebugToolsConfig.UseIBL) configuration.push_back("USE_IBL");

		GFX::Cmd::BindShader(state, m_GeometryShader.get(), VS | PS, configuration, true);
		VertPipeline->Draw(context, state, renderGroup);
	}

	GFX::Cmd::MarkerEnd(context);
}
