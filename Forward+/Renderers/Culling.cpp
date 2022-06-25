#include "Culling.h"

#include <Engine/Render/Buffer.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Shader.h>
#include <Engine/System/ApplicationConfiguration.h>

#include "Renderers/Util/ConstantManager.h"
#include "Gui/GUI_Implementations.h"
#include "Scene/SceneGraph.h"
#include "Shaders/shared_definitions.h"

void Culling::Init(ID3D11DeviceContext* context)
{
	m_LightCullingShader = GFX::CreateShader("Forward+/Shaders/light_culling.hlsl");
	UpdateResources(context);
}

void Culling::UpdateResources(ID3D11DeviceContext* context)
{
	if (m_VisibleLightsBuffer.Valid())
		GFX::Storage::Free(m_VisibleLightsBuffer);

	m_NumTilesX = MathUtility::CeilDiv(AppConfig.WindowWidth, TILE_SIZE);
	m_NumTilesY = MathUtility::CeilDiv(AppConfig.WindowHeight, TILE_SIZE);
	m_VisibleLightsBuffer = GFX::CreateBuffer(m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_SB | RCF_Bind_UAV);
}

namespace CullingPrivate
{
	bool IsVisible(const Drawable& d)
	{
		const Entity& e = MainSceneGraph.Entities[d.EntityIndex];
		const ViewFrustum& vf = MainSceneGraph.MainCamera.CameraFrustum;
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

void Culling::CullGeometries(ID3D11DeviceContext* context)
{
	if (!DebugToolsConfig.FreezeGeometryCulling)
	{
		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			RenderGroup& rg = MainSceneGraph.RenderGroups[i];
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

void Culling::CullLights(ID3D11DeviceContext* context, TextureID depth)
{
	if (!DebugToolsConfig.FreezeLightCulling && !DebugToolsConfig.DisableLightCulling)
	{
		std::vector<std::string> config{};
		if (PostprocessSettings.AntialiasingMode == AntiAliasingMode::MSAA) config.push_back("MULTISAMPLE_DEPTH");

		CBManager.Clear();
		CBManager.Add(MainSceneGraph.SceneInfoData);
		CBManager.Add(MainSceneGraph.MainCamera.CameraData, true);

		GFX::Cmd::BindRenderTarget(context, TextureID{}, TextureID{});
		GFX::Cmd::MarkerBegin(context, "Light Culling");
		GFX::Cmd::BindShader<CS>(context, m_LightCullingShader, config);
		GFX::Cmd::BindSRV<CS>(context, MainSceneGraph.Lights.GetBuffer(), 0);
		GFX::Cmd::BindSRV<CS>(context, depth, 1);
		GFX::Cmd::BindUAV<CS>(context, m_VisibleLightsBuffer, 0);
		context->Dispatch(m_NumTilesX, m_NumTilesY, 1);
		GFX::Cmd::BindUAV<CS>(context, nullptr, 0);
		GFX::Cmd::BindSRV<CS>(context, nullptr, 1);
		GFX::Cmd::MarkerEnd(context);
	}
}
