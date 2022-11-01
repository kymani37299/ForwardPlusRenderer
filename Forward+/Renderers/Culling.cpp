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
	m_CullingStatsBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_UAV));
	m_CullingStatsReadback = ScopedRef<Buffer>(GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Readback));
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
		const Entity& e = MainSceneGraph->Entities[d.EntityIndex];
		const ViewFrustum& vf = MainSceneGraph->MainCamera.CameraFrustum;
		const BoundingSphere bv = e.GetBoundingVolume();
		return vf.IsInFrustum(bv);
	}
}

void Culling::CullGeometries(GraphicsContext& context, Texture* depth)
{
	if (!RenderSettings.Culling.GeometryCullingFrozen)
	{
		UpdateStats(context);

		// Clear stats buffer
		uint32_t clearValue = 0;
		GFX::Cmd::UploadToBufferGPU(context, m_CullingStatsBuffer.get(), 0, &clearValue, 0, sizeof(uint32_t));

		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			if (RenderSettings.Culling.GeometryCullingOnCPU)
			{
				CullRenderGroupCPU(context, MainSceneGraph->RenderGroups[i]);
			}
			else
			{
				CullRenderGroupGPU(context, MainSceneGraph->RenderGroups[i], depth);
			}
		}
	}
}

void Culling::CullLights(GraphicsContext& context, Texture* depth)
{
	if (RenderSettings.Culling.LightCullingEnabled)
	{
		GraphicsState state{};
		state.Table.CBVs.resize(1);
		state.Table.SRVs.resize(2);
		state.Table.UAVs.resize(1);

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
		GFX::Cmd::BindState(context, state);
		context.CmdList->Dispatch(m_NumTilesX, m_NumTilesY, 1);
		GFX::Cmd::MarkerEnd(context);
	}
}

void Culling::CullRenderGroupCPU(GraphicsContext& context, RenderGroup& rg)
{
	if (rg.Drawables.GetSize() == 0) return;

	const uint32_t numDrawables = (uint32_t) rg.Drawables.GetSize();
	rg.VisibilityMask = BitField{ numDrawables };

	for (uint32_t i = 0; i < numDrawables; i++)
	{
		const Drawable& d = rg.Drawables[i];
		if (!RenderSettings.Culling.GeometryCullingEnabled || 
			(d.DrawableIndex != Drawable::InvalidIndex && CullingPrivate::IsVisible(d)))
		{
			rg.VisibilityMask.Set(i, true);
		}
	}
}

void Culling::CullRenderGroupGPU(GraphicsContext& context, RenderGroup& rg, Texture* depth)
{
	if (rg.Drawables.GetSize() == 0u) return;

	std::vector<std::string> config{};
	config.push_back("GEO_CULLING");
	if (!RenderSettings.Culling.GeometryCullingEnabled) config.push_back("FORCE_VISIBLE");
	if (!RenderSettings.Culling.GeometryOcclusionCullingEnabled) config.push_back("DISABLE_OCCLUSION_CULLING");

	GFX::Cmd::MarkerBegin(context, "Geometry Culling");

	GFX::ExpandBuffer(context, rg.VisibilityMaskBuffer.get(), rg.Drawables.GetSize() * sizeof(uint32_t));

	GraphicsState cullingState{};
	cullingState.Table.SRVs.push_back(rg.Drawables.GetBuffer());
	cullingState.Table.SRVs.push_back(MainSceneGraph->Entities.GetBuffer());
	cullingState.Table.SRVs.push_back(depth);
	cullingState.Table.UAVs.push_back(rg.VisibilityMaskBuffer.get());
	cullingState.Table.UAVs.push_back(m_CullingStatsBuffer.get());

	CBManager.Clear();
	CBManager.Add(MainSceneGraph->MainCamera.CameraData);
	CBManager.Add(MainSceneGraph->MainCamera.CameraFrustum);
	CBManager.Add(rg.Drawables.GetSize());
	// CBManager.Add(AppConfig.WindowWidth);
	// CBManager.Add(AppConfig.WindowHeight);
	cullingState.Table.CBVs.push_back(CBManager.GetBuffer());
	cullingState.Shader = m_GeometryCullingShader.get();
	cullingState.ShaderStages = CS;
	cullingState.ShaderConfig = config;
	GFX::Cmd::BindState(context, cullingState);

	context.CmdList->Dispatch((UINT) MathUtility::CeilDiv(rg.Drawables.GetSize(), (uint32_t) WAVESIZE), 1u, 1u);

	GFX::Cmd::CopyToBuffer(context, m_CullingStatsBuffer.get(), 0, m_CullingStatsReadback.get(), 0, sizeof(uint32_t));

	GFX::Cmd::MarkerEnd(context);
}

void Culling::UpdateStats(GraphicsContext& context)
{
	if (RenderSettings.Culling.GeometryCullingOnCPU)
	{
		RenderStats.TotalDrawables = 0;
		RenderStats.VisibleDrawables = 0;
		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			RenderGroup& rg = MainSceneGraph->RenderGroups[i];
			RenderStats.TotalDrawables += rg.Drawables.GetSize();
			RenderStats.VisibleDrawables += rg.VisibilityMask.CountOnes();
		}
	}
	else
	{
		RenderStats.TotalDrawables = 0;
		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++) RenderStats.TotalDrawables += MainSceneGraph->RenderGroups[i].Drawables.GetSize();

		void* visibleDrawablesPtr = nullptr;
		m_CullingStatsReadback->Handle->Map(0, nullptr, &visibleDrawablesPtr);
		if(visibleDrawablesPtr) RenderStats.VisibleDrawables = *static_cast<uint32_t*>(visibleDrawablesPtr);
		m_CullingStatsReadback->Handle->Unmap(0, nullptr);
	}
}
