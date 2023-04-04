#include "HzbGenerator.h"

#include <Engine/Render/Buffer.h>
#include <Engine/Render/Context.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Texture.h>
#include <Engine/Render/Shader.h>
#include <Engine/Utility/MathUtility.h>

#include "Renderers/Util/ConstantBuffer.h"
#include "Scene/SceneGraph.h"
#include "Shaders/shared_definitions.h"

void HZBGenerator::Init(GraphicsContext& context)
{
	m_GenerateHZBShader = ScopedRef<Shader>(new Shader{ "Forward+/Shaders/generate_hzb.hlsl" });
}

Texture* HZBGenerator::GetHZB(GraphicsContext& context, Texture* depth, const Camera& camera)
{
	PROFILE_SECTION(context, "Generate HZB");

	if (!m_ReprojectedDepth || m_ReprojectedDepth->Width != depth->Width || m_ReprojectedDepth->Height != depth->Height)
	{
		RecreateTextures(context, depth->Width, depth->Height);
	}

	// Clear depth
	{
		GraphicsState state{};
		state.Table.UAVs[0] = m_ReprojectedDepth.get();
		state.Shader = m_GenerateHZBShader.get();
		state.ShaderConfig = { "CLEAR_DEPTH" };
		state.ShaderStages = CS;
		state.PushConstantCount = 2;
		context.ApplyState(state);

		PushConstantTable pushConstants;
		pushConstants[0].Uint = m_ReprojectedDepth->Width;
		pushConstants[1].Uint = m_ReprojectedDepth->Height;
		GFX::Cmd::SetPushConstants(CS, context, pushConstants);

		GFX::Cmd::Dispatch(context, (UINT)MathUtility::CeilDiv(m_ReprojectedDepth->Width, OPT_TILE_SIZE), (UINT)MathUtility::CeilDiv(m_ReprojectedDepth->Height, OPT_TILE_SIZE), 1);
	}

	// Reproject depth
	{
		ConstantBuffer cb{};
		cb.Add(depth->Width);
		cb.Add(depth->Height);
		cb.Add(0); // Padding
		cb.Add(0);
		cb.Add(camera.CameraData);
		cb.Add(camera.LastCameraData);

		GraphicsState state{};
		state.Table.CBVs[0] = cb.GetBuffer(context);
		state.Table.SRVs[0] = depth;
		state.Table.UAVs[0] = m_ReprojectedDepth.get();
		state.Shader = m_GenerateHZBShader.get();
		state.ShaderConfig = { "REPROJECT_DEPTH" };
		state.ShaderStages = CS;
		context.ApplyState(state);

		GFX::Cmd::Dispatch(context, (UINT)MathUtility::CeilDiv(depth->Width, OPT_TILE_SIZE), (UINT)MathUtility::CeilDiv(depth->Height, OPT_TILE_SIZE), 1);
	}

	// Generate HZB
	{
		// Get subresources
		std::vector<Texture*> hzbMips{};
		hzbMips.resize(m_HZBMips);
		for(uint32_t i=0;i<m_HZBMips;i++)
			hzbMips[i] = GFX::GetTextureSubresource(m_HZB.get(), i, i, 0, 0);

		// Common state
		GraphicsState state{};
		state.Table.SMPs[0] = { D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
		state.Shader = m_GenerateHZBShader.get();
		state.ShaderConfig = { "GENERATE_HZB" };
		state.ShaderStages = CS;
		state.PushConstantCount = 4;
		
		PushConstantTable pushConstants;
		pushConstants[2].Uint = depth->Width;
		pushConstants[3].Uint = depth->Height;

		for (uint32_t writeMip = 0; writeMip < m_HZBMips; writeMip++)
		{
			state.Table.SRVs[0] = writeMip == 0 ? m_ReprojectedDepth.get() : hzbMips[writeMip - 1];
			state.Table.UAVs[0] = hzbMips[writeMip];
			context.ApplyState(state);

			pushConstants[0].Uint = writeMip == 0 ? 0 : writeMip - 1;
			pushConstants[1].Uint = writeMip;
			GFX::Cmd::SetPushConstants(CS, context, pushConstants);

			const uint32_t mipWidth = depth->Width >> writeMip;
			const uint32_t mipHeight = depth->Height >> writeMip;
			GFX::Cmd::Dispatch(context, (UINT)MathUtility::CeilDiv(mipWidth, OPT_TILE_SIZE), (UINT)MathUtility::CeilDiv(mipHeight, OPT_TILE_SIZE), 1);
		}
	
		// Bring back state of a parent resource
		// And release subresources
		for (uint32_t i = 0; i < m_HZBMips; i++)
		{
			GFX::Cmd::TransitionResource(context, hzbMips[i], m_HZB->CurrState);
			delete hzbMips[i];
		}
			
	}

	return m_HZB.get();
}

void HZBGenerator::RecreateTextures(GraphicsContext& context, uint32_t width, uint32_t height)
{
	m_ReprojectedDepth = ScopedRef<Texture>(GFX::CreateTexture(width, height, RCF::UAV, 1, DXGI_FORMAT_R32_FLOAT));

	m_HZBMips = (uint32_t) log2(MAX(width, height));
	m_HZB = ScopedRef<Texture>(GFX::CreateTexture(width, height, RCF::UAV, m_HZBMips, DXGI_FORMAT_R32_FLOAT));
}
