#include "Culling.h"

#include <Engine/Render/Resource.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Texture.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Shader.h>
#include <Engine/System/ApplicationConfiguration.h>
#include <Engine/Utility/MathUtility.h>

#include "Globals.h"
#include "Renderers/Util/ConstantBuffer.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneGraph.h"
#include "Shaders/shared_definitions.h"

struct CullingStatsSB
{
	uint32_t TotalDrawables = 0;
	uint32_t VisibleDrawables = 0;

	uint32_t TotalTriangles = 0;
	uint32_t VisibleTriangles = 0;
};

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
	m_VisibleLightsBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t), sizeof(uint32_t), RCF::UAV));
	GFX::SetDebugName(m_VisibleLightsBuffer.get(), "Culling::VisibleLightsBuffer");
}

namespace CullingPrivate
{
	bool IsVisible(const Drawable& d)
	{
		const ViewFrustum& vf = SceneManager::Get().GetSceneGraph().MainCamera.CameraFrustum;
		const BoundingSphere bv = d.GetBoundingVolume();
		return vf.IsInFrustum(bv);
	}
}

void Culling::CullGeometries(GraphicsContext& context, GeometryCullingInput& input)
{
	PROFILE_SECTION(context, "Cull geometries")

	UpdateStats(context, input.Cam.CullingData);

	GFX::Cmd::ClearBuffer(context, input.Cam.CullingData.StatsBuffer->GetWriteBuffer());

	for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
	{
		const RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
		if (RenderSettings.Culling.GeoCullingMode == GeometryCullingMode::CPU_FrustumCulling)
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
		PROFILE_SECTION(context, "Light culling");

		GraphicsState state{};
		
		ConstantBuffer cb{};
		cb.Add(SceneManager::Get().GetSceneGraph().SceneInfoData);
		cb.Add(SceneManager::Get().GetSceneGraph().MainCamera.CameraData);

		state.Table.CBVs[0] = cb.GetBuffer(context);
		state.Table.SRVs[0] = SceneManager::Get().GetSceneGraph().Lights.GetBuffer();
		state.Table.SRVs[1] = depth;
		state.Table.UAVs[0] = m_VisibleLightsBuffer.get();
		state.Shader = m_LightCullingShader.get();
		state.ShaderStages = CS;

		context.ApplyState(state);
		GFX::Cmd::Dispatch(context, m_NumTilesX, m_NumTilesY, 1);
	}
}

void Culling::CullRenderGroupCPU(GraphicsContext& context, RenderGroupType rgType, GeometryCullingInput& input)
{
	RenderGroup& rg = SceneManager::Get().GetSceneGraph().RenderGroups[EnumToInt(rgType)];
	RenderGroupCullingData& cullData = input.Cam.CullingData[rgType];

	if (rg.Drawables.GetSize() == 0) return;

	const uint32_t numDrawables = (uint32_t) rg.Drawables.GetSize();
	cullData.VisibilityMask = BitField{numDrawables};

	for (uint32_t i = 0; i < numDrawables; i++)
	{
		const Drawable& d = rg.Drawables[i];
		if (RenderSettings.Culling.GeoCullingMode == GeometryCullingMode::None || 
			(d.DrawableIndex != Drawable::InvalidIndex && CullingPrivate::IsVisible(d)))
		{
			cullData.VisibilityMask.Set(i, true);
		}
	}
}

void Culling::CullRenderGroupGPU(GraphicsContext& context, RenderGroupType rgType, GeometryCullingInput& input)
{
	RenderGroup& rg = SceneManager::Get().GetSceneGraph().RenderGroups[EnumToInt(rgType)];
	CameraCullingData& camCullData = input.Cam.CullingData;
	RenderGroupCullingData& cullData = camCullData[rgType];

	if (rg.Drawables.GetSize() == 0u) return;

	std::vector<std::string> config{};
	config.push_back("GEO_CULLING");
	if (RenderSettings.Culling.GeoCullingMode == GeometryCullingMode::None) config.push_back("FORCE_VISIBLE");
	
	if (input.HZB && RenderSettings.Culling.GeoCullingMode == GeometryCullingMode::GPU_OcclusionCulling)
	{
		config.push_back("OCCLUSION_CULLING");
	}

	const DeviceSpecification& deviceSpec = Device::Get()->GetSpec();
	if (deviceSpec.SupportWaveIntrinscs && deviceSpec.WavefrontSize >= 32)
	{
		config.push_back("WAVEFRONT_SIZE_GE_32");
	}

	if(!cullData.VisibilityMaskBuffer) cullData.VisibilityMaskBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(1, 1, RCF::UAV | RCF::RAW));

	constexpr uint32_t BitsPerByte = 8;
	GFX::ExpandBuffer(context, cullData.VisibilityMaskBuffer.get(), MathUtility::CeilDiv(rg.Drawables.GetSize(), BitsPerByte));

	GraphicsState cullingState{};
	cullingState.Table.SRVs[0] = rg.Drawables.GetBuffer();
	cullingState.Table.SRVs[1] = input.HZB;
	cullingState.Table.SRVs[2] = rg.Meshes.GetBuffer();
	cullingState.Table.UAVs[0] = cullData.VisibilityMaskBuffer.get();
	cullingState.Table.UAVs[1] = camCullData.StatsBuffer->GetWriteBuffer();
	cullingState.Table.SMPs[0] = { D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER, {1.0f, 1.0f, 1.0f, 1.0f} };

	ConstantBuffer cb{};
	cb.Add(input.Cam.CameraData);
	cb.Add(input.Cam.CameraFrustum);
	
	cullingState.Table.CBVs[0] = cb.GetBuffer(context);
	cullingState.Shader = m_GeometryCullingShader.get();
	cullingState.ShaderStages = CS;
	cullingState.ShaderConfig = config;
	cullingState.PushConstantCount = 3;
	context.ApplyState(cullingState);

	PushConstantTable pushConstants;
	pushConstants[0].Uint = rg.Drawables.GetSize();
	pushConstants[1].Uint = input.HZB ? input.HZB->Width : 0;
	pushConstants[2].Uint = input.HZB ? input.HZB->Height : 0;
	
	GFX::Cmd::SetPushConstants(CS, context, pushConstants);
	GFX::Cmd::Dispatch(context, (UINT) MathUtility::CeilDiv(rg.Drawables.GetSize(), (uint32_t) OPT_COMP_TG_SIZE), 1u, 1u);

	GFX::Cmd::AddReadbackRequest(context, camCullData.StatsBuffer.get());

}

void Culling::UpdateStats(GraphicsContext& context, CameraCullingData& cullingData)
{
	if (!cullingData.StatsBuffer)
	{
		cullingData.StatsBuffer = ScopedRef<ReadbackBuffer>(new ReadbackBuffer{ sizeof(CullingStatsSB) });
	}

	CullingStatistics& cullStats = cullingData.CullingStats;
	cullStats.TotalDrawables = 0;
	cullStats.VisibleDrawables = 0;
	cullStats.TotalTriangles = 0;
	cullStats.VisibleTriangles = 0;

	switch (RenderSettings.Culling.GeoCullingMode)
	{
	case GeometryCullingMode::None:
	{
		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
			RenderGroup& rg = SceneManager::Get().GetSceneGraph().RenderGroups[i];

			cullStats.TotalDrawables += rg.Drawables.GetSize();
			cullStats.VisibleDrawables += rg.Drawables.GetSize();
		}
	} break;
	case GeometryCullingMode::CPU_FrustumCulling:
	{
		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
			RenderGroup& rg = SceneManager::Get().GetSceneGraph().RenderGroups[i];

			cullStats.TotalDrawables += rg.Drawables.GetSize();
			cullStats.VisibleDrawables += cullingData[rgType].VisibilityMask.CountOnes();
		}
	} break;
	case GeometryCullingMode::GPU_FrustumCulling:
	case GeometryCullingMode::GPU_OcclusionCulling:
	{
		Buffer* readBuffer = cullingData.StatsBuffer->GetReadBuffer();
		void* cullingStatsDataPtr = nullptr;
		readBuffer->Handle->Map(0, nullptr, &cullingStatsDataPtr);
		if (cullingStatsDataPtr)
		{
			const CullingStatsSB* cullingStatsPtr = reinterpret_cast<const CullingStatsSB*>(cullingStatsDataPtr);
			cullStats.TotalDrawables = cullingStatsPtr->TotalDrawables;
			cullStats.VisibleDrawables = cullingStatsPtr->VisibleDrawables;
			cullStats.TotalTriangles = cullingStatsPtr->TotalTriangles;
			cullStats.VisibleTriangles = cullingStatsPtr->VisibleTriangles;
		}
		readBuffer->Handle->Unmap(0, nullptr);
	} break;
	case GeometryCullingMode::Count:
	default: NOT_IMPLEMENTED;
	}
}
