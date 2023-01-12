#include "Culling.h"

#include <Engine/Render/Resource.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Texture.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Shader.h>
#include <Engine/System/ApplicationConfiguration.h>
#include <Engine/Utility/MathUtility.h>

#include "Globals.h"
#include "Renderers/Util/ConstantManager.h"
#include "Scene/SceneGraph.h"
#include "Shaders/shared_definitions.h"

Culling::Culling()
{
}

Culling::~Culling()
{
}

void Culling::Init(GraphicsContext& context)
{
	m_LightCullingShader = ScopedRef<Shader>(new Shader{ "Forward+/Shaders/light_culling.hlsl" });
	m_GeometryCullingShader = ScopedRef<Shader>(new Shader{ "Forward+/Shaders/geometry_culling.hlsl" });
	UpdateResources(context);
}

void Culling::UpdateResources(GraphicsContext& context)
{
	m_NumTilesX = MathUtility::CeilDiv(AppConfig.WindowWidth, (uint32_t) TILE_SIZE);
	m_NumTilesY = MathUtility::CeilDiv(AppConfig.WindowHeight, (uint32_t) TILE_SIZE);
	m_VisibleLightsBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_UAV));
	GFX::SetDebugName(m_VisibleLightsBuffer.get(), "Culling::VisibleLightsBuffer");
}

namespace CullingPrivate
{
	bool IsVisible(const Drawable& d)
	{
		const ViewFrustum& vf = MainSceneGraph->MainCamera.CameraFrustum;
		const BoundingSphere bv = d.GetBoundingVolume();
		return vf.IsInFrustum(bv);
	}
}

void Culling::CullGeometries(GraphicsContext& context, GeometryCullingInput& input)
{
	UpdateStats(context, input.Cam.CullingData);

	// Clear stats buffer
	uint32_t clearValue = 0;
	GFX::Cmd::UploadToBufferGPU(context, input.Cam.CullingData.StatsBuffer.get(), 0, &clearValue, 0, sizeof(uint32_t));

	for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
	{
		const RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
		if (RenderSettings.Culling.GeometryCullingOnCPU)
		{
			CullRenderGroupCPU(context, rgType, input);
		}
		else
		{
			CullRenderGroupGPU(context, rgType, input);
		}
	}
}

void Culling::CullLights(GraphicsContext& context, Texture* depth)
{
	if (RenderSettings.Culling.LightCullingEnabled)
	{
		GraphicsState state{};
		
		CBManager.Clear();
		CBManager.Add(MainSceneGraph->SceneInfoData);
		CBManager.Add(MainSceneGraph->MainCamera.CameraData);

		state.Table.CBVs[0] = CBManager.GetBuffer();
		state.Table.SRVs[0] = MainSceneGraph->Lights.GetBuffer();
		state.Table.SRVs[1] = depth;
		state.Table.UAVs[0] = m_VisibleLightsBuffer.get();
		state.Shader = m_LightCullingShader.get();
		state.ShaderStages = CS;

		GFX::Cmd::MarkerBegin(context, "Light Culling");
		context.ApplyState(state);
		context.CmdList->Dispatch(m_NumTilesX, m_NumTilesY, 1);
		GFX::Cmd::MarkerEnd(context);
	}
}

void Culling::CullRenderGroupCPU(GraphicsContext& context, RenderGroupType rgType, GeometryCullingInput& input)
{
	RenderGroup& rg = MainSceneGraph->RenderGroups[EnumToInt(rgType)];
	RenderGroupCullingData& cullData = input.Cam.CullingData[rgType];

	if (rg.Drawables.GetSize() == 0) return;

	const uint32_t numDrawables = (uint32_t) rg.Drawables.GetSize();
	cullData.VisibilityMask = BitField{numDrawables};

	for (uint32_t i = 0; i < numDrawables; i++)
	{
		const Drawable& d = rg.Drawables[i];
		if (!RenderSettings.Culling.GeometryCullingEnabled || 
			(d.DrawableIndex != Drawable::InvalidIndex && CullingPrivate::IsVisible(d)))
		{
			cullData.VisibilityMask.Set(i, true);
		}
	}
}

void Culling::CullRenderGroupGPU(GraphicsContext& context, RenderGroupType rgType, GeometryCullingInput& input)
{
	RenderGroup& rg = MainSceneGraph->RenderGroups[EnumToInt(rgType)];
	CameraCullingData& camCullData = input.Cam.CullingData;
	RenderGroupCullingData& cullData = camCullData[rgType];

	if (rg.Drawables.GetSize() == 0u) return;

	std::vector<std::string> config{};
	config.push_back("GEO_CULLING");
	if (!RenderSettings.Culling.GeometryCullingEnabled) config.push_back("FORCE_VISIBLE");
	if (!RenderSettings.Culling.GeometryOcclusionCullingEnabled) config.push_back("DISABLE_OCCLUSION_CULLING");

	if(!cullData.VisibilityMaskBuffer) cullData.VisibilityMaskBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_UAV));
	GFX::ExpandBuffer(context, cullData.VisibilityMaskBuffer.get(), rg.Drawables.GetSize() * sizeof(uint32_t));

	GraphicsState cullingState{};
	cullingState.Table.SRVs[0] = rg.Drawables.GetBuffer();
	cullingState.Table.SRVs[1] = input.Depth;
	cullingState.Table.UAVs[0] = cullData.VisibilityMaskBuffer.get();
	cullingState.Table.UAVs[1] = camCullData.StatsBuffer.get();

	CBManager.Clear();
	CBManager.Add(input.Cam.CameraData);
	CBManager.Add(input.Cam.CameraFrustum);
	CBManager.Add(rg.Drawables.GetSize());
	cullingState.Table.CBVs[0] = CBManager.GetBuffer();
	cullingState.Shader = m_GeometryCullingShader.get();
	cullingState.ShaderStages = CS;
	cullingState.ShaderConfig = config;
	context.ApplyState(cullingState);

	context.CmdList->Dispatch((UINT) MathUtility::CeilDiv(rg.Drawables.GetSize(), (uint32_t) WAVESIZE), 1u, 1u);

	GFX::Cmd::CopyToBuffer(context, camCullData.StatsBuffer.get(), 0, camCullData.StatsBufferReadback.get(), 0, sizeof(uint32_t));
}

void Culling::UpdateStats(GraphicsContext& context, CameraCullingData& cullingData)
{
	if (!cullingData.StatsBuffer)
	{
		cullingData.StatsBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_UAV));
		cullingData.StatsBufferReadback = ScopedRef<Buffer>(GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Readback));
	}

	if (RenderSettings.Culling.GeometryCullingOnCPU)
	{
		cullingData.TotalElements = 0;
		cullingData.VisibleElements = 0;
		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
			RenderGroup& rg = MainSceneGraph->RenderGroups[i];

			cullingData.TotalElements += rg.Drawables.GetSize();
			cullingData.VisibleElements += cullingData[rgType].VisibilityMask.CountOnes();
		}
	}
	else
	{
		cullingData.TotalElements = 0;
		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++) cullingData.TotalElements += MainSceneGraph->RenderGroups[i].Drawables.GetSize();

		void* visibleDrawablesPtr = nullptr;
		cullingData.StatsBufferReadback->Handle->Map(0, nullptr, &visibleDrawablesPtr);
		if(visibleDrawablesPtr) cullingData.VisibleElements = *static_cast<uint32_t*>(visibleDrawablesPtr);
		cullingData.StatsBufferReadback->Handle->Unmap(0, nullptr);
	}
}
