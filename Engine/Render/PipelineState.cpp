#include "PipelineState.h"

#include <unordered_map>

#include "Render/Device.h"
#include "Utility/Hash.h"

namespace GFX
{
	static std::unordered_map<uint32_t, CompiledPipelineState> Cache;

	PipelineState DefaultPipelineState()
	{
		PipelineState ps{};

		ps.RS.CullMode = D3D11_CULL_NONE;
		ps.RS.FillMode = D3D11_FILL_SOLID;
        ps.RS.AntialiasedLineEnable = false;
        ps.RS.DepthBias = 0;
        ps.RS.DepthBiasClamp = 0.0f;
        ps.RS.DepthClipEnable = true;
        ps.RS.FrontCounterClockwise = false;
        ps.RS.MultisampleEnable = false;
        ps.RS.ScissorEnable = false;
        ps.RS.SlopeScaledDepthBias = false;

		ps.DS.DepthEnable = false;
        ps.DS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        ps.DS.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		ps.DS.StencilEnable = false;
        ps.DS.StencilReadMask = 0xff;
        ps.DS.StencilWriteMask = 0xff;
        ps.DS.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        ps.DS.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        ps.DS.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
        ps.DS.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        ps.DS.BackFace = ps.DS.FrontFace;

        ps.BS.AlphaToCoverageEnable = false;
        ps.BS.IndependentBlendEnable = false;
        for (uint16_t i = 0; i < 8; i++)
        {
            ps.BS.RenderTarget[i].BlendEnable = false;
            ps.BS.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            ps.BS.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
            ps.BS.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            ps.BS.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
            ps.BS.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            ps.BS.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
            ps.BS.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
        }
		
		return ps;
	}

	const CompiledPipelineState& CompilePipelineState(const PipelineState& pipelineState)
	{
        uint32_t hash = Hash::Crc32(pipelineState);

		if (Cache.find(hash) == Cache.end())
		{
			CompiledPipelineState cps{};

			API_CALL(Device::Get()->GetHandle()->CreateRasterizerState(&pipelineState.RS, cps.RS.GetAddressOf()));
			API_CALL(Device::Get()->GetHandle()->CreateDepthStencilState(&pipelineState.DS, cps.DS.GetAddressOf()));
			API_CALL(Device::Get()->GetHandle()->CreateBlendState(&pipelineState.BS, cps.BS.GetAddressOf()));

			Cache[hash] = cps;
		}

		return Cache[hash];
	}
}