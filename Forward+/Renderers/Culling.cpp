#include "Culling.h"

#include <Engine/Render/Resource.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Texture.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Shader.h>
#include <Engine/System/ApplicationConfiguration.h>

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
	UpdateResources(context);
}

void Culling::UpdateResources(GraphicsContext& context)
{
	m_NumTilesX = MathUtility::CeilDiv(AppConfig.WindowWidth, TILE_SIZE);
	m_NumTilesY = MathUtility::CeilDiv(AppConfig.WindowHeight, TILE_SIZE);
	m_VisibleLightsBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_UAV));
	GFX::SetDebugName(m_VisibleLightsBuffer.get(), "Culling::VisibleLightsBuffer");
}

namespace CullingPrivate
{
	bool IsVisible(const Drawable& d)
	{
		const Entity& e = MainSceneGraph->Entities[d.EntityIndex];
		const ViewFrustum& vf = MainSceneGraph->MainCamera.CameraFrustum;
		const BoundingSphere bv = e.GetBoundingVolume(d.BoundingVolume);

		return vf.IsInFrustum(bv);
	}

	void CullRenderGroup(RenderGroup& rg)
	{
		const uint32_t numDrawables = rg.Drawables.GetSize();
		rg.VisibilityMask = BitField{ numDrawables };
		for (uint32_t i = 0; i < numDrawables; i++)
		{
			Drawable& d = rg.Drawables[i];
			if (d.DrawableIndex != Drawable::InvalidIndex && IsVisible(d))
			{
				rg.VisibilityMask.Set(i, true);
			}
		}
	}
}

void Culling::CullGeometries(GraphicsContext& context)
{
	if (!DebugToolsConfig.FreezeGeometryCulling)
	{
		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			RenderGroup& rg = MainSceneGraph->RenderGroups[i];
			if (DebugToolsConfig.DisableGeometryCulling)
			{
				uint32_t numDrawables = rg.Drawables.GetSize();
				rg.VisibilityMask = BitField(numDrawables);
				for (uint32_t i = 0; i < numDrawables; i++) rg.VisibilityMask.Set(i, true);
			}
			else
			{
				CullingPrivate::CullRenderGroup(rg);
			}
		}
	}
}

void Culling::CullLights(GraphicsContext& context, Texture* depth)
{
	if (!DebugToolsConfig.FreezeLightCulling && !DebugToolsConfig.DisableLightCulling)
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

		GFX::Cmd::MarkerBegin(context, "Light Culling");
		GFX::Cmd::BindShader(state, m_LightCullingShader.get(), CS);
		GFX::Cmd::BindState(context, state);
		context.CmdList->Dispatch(m_NumTilesX, m_NumTilesY, 1);
		GFX::Cmd::MarkerEnd(context);
	}
}
