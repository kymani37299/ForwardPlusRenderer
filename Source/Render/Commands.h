#pragma once

#include <vector>

#include "Render/RenderAPI.h"
#include "Render/ResourceID.h"
#include "Render/PipelineState.h"

namespace GFX
{
	namespace Cmd
	{
		void ResetContext(ID3D11DeviceContext1* context);

		void BindShader(ID3D11DeviceContext1* context, ShaderID shaderID, bool multiInput = false);
		void BindVertexBuffer(ID3D11DeviceContext1* context, BufferID bufferID);
		void BindVertexBuffers(ID3D11DeviceContext1* context, std::vector<BufferID> buffers);
		void BindIndexBuffer(ID3D11DeviceContext1* context, BufferID bufferID);
		void BindRenderTarget(ID3D11DeviceContext1* context, RenderTargetID rtID);
		void BindTextureSRV(ID3D11DeviceContext1* context, TextureID textureID, uint32_t slot);

		void SetPipelineState(ID3D11DeviceContext1* context, const PipelineState& pipelineState);
		void SetPipelineState(ID3D11DeviceContext1* context, const CompiledPipelineState& pipelineState);
	}
}