#include "GeometryRenderer.h"

#include <Engine/Render/Commands.h>

#include "Gui/GUI_Implementations.h"
#include "Renderers/Util/ConstantManager.h"
#include "Renderers/Util/SamplerManager.h"
#include "Renderers/Util/VertexPipeline.h"
#include "Scene/SceneGraph.h"

void GeometryRenderer::Init(ID3D11DeviceContext* context)
{
	m_DepthPrepassShader = GFX::CreateShader("Forward+/Shaders/depth.hlsl");
	m_GeometryShader = GFX::CreateShader("Forward+/Shaders/geometry.hlsl");
}

void GeometryRenderer::DepthPrepass(ID3D11DeviceContext* context)
{
	const bool drawMotionVectors = PostprocessSettings.AntialiasingMode == AntiAliasingMode::TAA;

	GFX::Cmd::MarkerBegin(context, "Depth Prepass");

	CBManager.Clear();
	CBManager.Add(MainSceneGraph.MainCamera.CameraData);
	CBManager.Add(MainSceneGraph.MainCamera.LastCameraData);
	CBManager.Add(MainSceneGraph.SceneInfoData, true);

	PipelineState pso = GFX::DefaultPipelineState();
	pso.DS.DepthEnable = true;

	RenderGroupType prepassTypes[] = { RenderGroupType::Opaque, RenderGroupType::AlphaDiscard };
	for (uint32_t i = 0; i < STATIC_ARRAY_SIZE(prepassTypes); i++)
	{
		RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
		RenderGroup& renderGroup = MainSceneGraph.RenderGroups[i];

		std::vector<std::string> config{};
		if (drawMotionVectors) config.push_back("MOTION_VECTORS");

		
		GFX::Cmd::SetPipelineState(context, pso);

		if (rgType == RenderGroupType::AlphaDiscard)
		{
			config.push_back("ALPHA_DISCARD");
			SSManager.Bind(context);
		}
		GFX::Cmd::BindShader<VS | PS>(context, m_DepthPrepassShader, config, true);
		VertPipeline.Draw(context, renderGroup);
	}
	GFX::Cmd::MarkerEnd(context);
}

void GeometryRenderer::Draw(ID3D11DeviceContext* context, TextureID shadowMask, BufferID visibleLights, TextureID irradianceMap)
{
	GFX::Cmd::MarkerBegin(context, "Geometry");

	CBManager.Clear();
	CBManager.Add(MainSceneGraph.MainCamera.CameraData);
	CBManager.Add(MainSceneGraph.SceneInfoData, true);

	PipelineState pso = GFX::DefaultPipelineState();
	pso.DS.DepthEnable = true;
	pso.DS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	pso.DS.StencilEnable = true;
	pso.DS.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	pso.DS.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	pso.DS.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	pso.DS.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	pso.DS.BackFace = pso.DS.FrontFace;
	GFX::Cmd::SetPipelineState(context, pso);

	SSManager.Bind(context);

	GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Lights.GetBuffer(), 0);
	GFX::Cmd::BindSRV<PS>(context, visibleLights, 1);
	GFX::Cmd::BindSRV<PS>(context, shadowMask, 2);
	GFX::Cmd::BindSRV<PS>(context, irradianceMap, 3);

	for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
	{
		RenderGroupType rgType = (RenderGroupType)i;
		RenderGroup& renderGroup = MainSceneGraph.RenderGroups[i];
		if (renderGroup.Drawables.GetSize() == 0) continue;

		std::vector<std::string> configuration;
		if (rgType == RenderGroupType::AlphaDiscard) configuration.push_back("ALPHA_DISCARD");
		if (DebugToolsConfig.DisableLightCulling) configuration.push_back("DISABLE_LIGHT_CULLING");
		if (DebugToolsConfig.UsePBR) configuration.push_back("USE_PBR");
		if (DebugToolsConfig.UsePBR && DebugToolsConfig.UseIBL) configuration.push_back("USE_IBL");

		GFX::Cmd::BindShader<VS | PS>(context, m_GeometryShader, configuration, true);
		VertPipeline.Draw(context, renderGroup);
	}

	GFX::Cmd::MarkerEnd(context);
}
