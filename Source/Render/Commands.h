#pragma once

#include <vector>

#include "Render/RenderAPI.h"
#include "Render/ResourceID.h"
#include "Render/PipelineState.h"

namespace GFX
{
	namespace Cmd
	{
		void MarkerBegin(ID3D11DeviceContext* context, const std::string& markerName);
		void MarkerEnd(ID3D11DeviceContext* context);
		void ResetContext(ID3D11DeviceContext1* context);

		void BindShader(ID3D11DeviceContext1* context, ShaderID shaderID, bool multiInput = false);
		void BindVertexBuffer(ID3D11DeviceContext1* context, BufferID bufferID);
		void BindVertexBuffers(ID3D11DeviceContext1* context, std::vector<BufferID> buffers);
		void BindIndexBuffer(ID3D11DeviceContext1* context, BufferID bufferID);
		void BindRenderTarget(ID3D11DeviceContext1* context, RenderTargetID rtID);

		void SetPipelineState(ID3D11DeviceContext1* context, const PipelineState& pipelineState);
		void SetPipelineState(ID3D11DeviceContext1* context, const CompiledPipelineState& pipelineState);

		void ClearRenderTarget(ID3D11DeviceContext1* context, RenderTargetID rtID);
		void UploadToBuffer(ID3D11DeviceContext1* context, BufferID bufferID, const void* data, uint64_t dataSize, uint64_t offset = 0);
	}
}