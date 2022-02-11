#pragma once

#include "Render/RenderAPI.h"

struct PipelineState
{
	D3D11_RASTERIZER_DESC RS;
	D3D11_DEPTH_STENCIL_DESC DS;
	D3D11_BLEND_DESC BS;
};

struct CompiledPipelineState
{
	ComPtr<ID3D11RasterizerState> RS;
	ComPtr<ID3D11DepthStencilState> DS;
	ComPtr<ID3D11BlendState> BS;
};

namespace GFX
{
	PipelineState DefaultPipelineState();
	const CompiledPipelineState& CompilePipelineState(const PipelineState& pipelineState);
}