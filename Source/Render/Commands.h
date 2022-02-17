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
		void ResetContext(ID3D11DeviceContext* context);
		ID3D11DeviceContext* CreateDeferredContext();
		void SubmitDeferredContext(ID3D11DeviceContext* context);

		void BindShader(ID3D11DeviceContext* context, ShaderID shaderID, bool multiInput = false);
		void BindVertexBuffer(ID3D11DeviceContext* context, BufferID bufferID);
		void BindVertexBuffers(ID3D11DeviceContext* context, std::vector<BufferID> buffers);
		void BindIndexBuffer(ID3D11DeviceContext* context, BufferID bufferID);
		void BindRenderTarget(ID3D11DeviceContext* context, TextureID colorID, TextureID depthID = TextureID{});

		void SetPipelineState(ID3D11DeviceContext* context, const PipelineState& pipelineState);
		void SetPipelineState(ID3D11DeviceContext* context, const CompiledPipelineState& pipelineState);

		void ClearRenderTarget(ID3D11DeviceContext* context, TextureID colorID, TextureID depthID = TextureID{});
		void UploadToBuffer(ID3D11DeviceContext* context, BufferID bufferID, const void* data, uint64_t dataSize, uint64_t offset = 0);
		void UploadToTexture(ID3D11DeviceContext* context, void* data, TextureID textureID, uint32_t mipIndex = 0);
		void CopyToTexture(ID3D11DeviceContext* context, TextureID srcTexture, TextureID dstTexture, uint32_t mipIndex = 0);
	}
}