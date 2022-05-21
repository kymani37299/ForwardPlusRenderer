#pragma once

#include <vector>

#include "Render/RenderAPI.h"
#include "Render/ResourceID.h"
#include "Render/Resource.h"
#include "Render/Texture.h"
#include "Render/PipelineState.h"

enum ShaderStage
{
	VS,
	PS,
	CS
};

namespace GFX
{
	namespace Cmd
	{
		void MarkerBegin(ID3D11DeviceContext* context, const std::string& markerName);
		void MarkerEnd(ID3D11DeviceContext* context);
		void ResetContext(ID3D11DeviceContext* context);
		ID3D11DeviceContext* CreateDeferredContext();
		void SubmitDeferredContext(ID3D11DeviceContext* context);

		void BindShader(ID3D11DeviceContext* context, ShaderID shaderID, std::vector<std::string> configuration = {}, bool multiInput = false);
		void BindVertexBuffer(ID3D11DeviceContext* context, BufferID bufferID);
		void BindVertexBuffers(ID3D11DeviceContext* context, std::vector<BufferID> buffers);
		void BindIndexBuffer(ID3D11DeviceContext* context, BufferID bufferID);
		void BindRenderTarget(ID3D11DeviceContext* context, TextureID colorID, TextureID depthID = TextureID{});
		void BindRenderTargets(ID3D11DeviceContext* context, std::vector<TextureID> colorID, TextureID depthID = TextureID{});

		void SetViewport(ID3D11DeviceContext* context, Float2 viewportSize);
		void SetPipelineState(ID3D11DeviceContext* context, const PipelineState& pipelineState);
		void SetPipelineState(ID3D11DeviceContext* context, const CompiledPipelineState& pipelineState);

		void ClearRenderTarget(ID3D11DeviceContext* context, TextureID colorID, TextureID depthID = TextureID{});
		void UploadToBuffer(ID3D11DeviceContext* context, BufferID bufferID, uint32_t dstOffset, const void* data, uint32_t srcOffset, size_t dataSize);
		void UploadToTexture(ID3D11DeviceContext* context, void* data, TextureID textureID, uint32_t mipIndex = 0);
		void CopyToTexture(ID3D11DeviceContext* context, TextureID srcTexture, TextureID dstTexture, uint32_t mipIndex = 0);
		void CopyToBuffer(ID3D11DeviceContext* context, BufferID srcBuffer, uint32_t srcOffset, BufferID dstBuffer, uint32_t dstOffset, uint32_t size);

		template<ShaderStage stage>
		void SetupStaticSamplers(ID3D11DeviceContext* context)
		{
			if constexpr (stage == VS) context->VSSetSamplers(0, GFX::GetStaticSamplersNum(), GFX::GetStaticSamplers());
			else if constexpr (stage == PS) context->PSSetSamplers(0, GFX::GetStaticSamplersNum(), GFX::GetStaticSamplers());
			else if constexpr (stage == CS) context->CSSetSamplers(0, GFX::GetStaticSamplersNum(), GFX::GetStaticSamplers());
			else NOT_IMPLEMENTED;
		}

		template<ShaderStage stage>
		void BindCBV(ID3D11DeviceContext* context, ID3D11Buffer* cbv, uint32_t slot)
		{
			if constexpr (stage == VS) context->VSSetConstantBuffers(slot, 1, &cbv);
			else if constexpr (stage == PS) context->PSSetConstantBuffers(slot, 1, &cbv);
			else if constexpr (stage == CS) context->CSSetConstantBuffers(slot, 1, &cbv);
			else NOT_IMPLEMENTED;
		}

		template<ShaderStage stage>
		void BindSRV(ID3D11DeviceContext* context, ID3D11ShaderResourceView* srv, uint32_t slot)
		{
			if constexpr (stage == VS) context->VSSetShaderResources(slot, 1, &srv);
			else if constexpr (stage == PS) context->PSSetShaderResources(slot, 1, &srv);
			else if constexpr (stage == CS) context->CSSetShaderResources(slot, 1, &srv);
			else NOT_IMPLEMENTED;
		}

		template<ShaderStage stage>
		void BindUAV(ID3D11DeviceContext* context, ID3D11UnorderedAccessView* uav, uint32_t slot)
		{
			if constexpr (stage == CS) context->CSSetUnorderedAccessViews(slot, 1, &uav, nullptr);
			else NOT_IMPLEMENTED;
		}

		template<ShaderStage stage>
		void BindCBV(ID3D11DeviceContext* context, BufferID bufferID, uint32_t slot)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			ID3D11Buffer* const cbv = buffer.Handle.Get();
			ASSERT(cbv, "BindCBV : Missing CBV");
			BindCBV<stage>(context, cbv, slot);
		}

		template<ShaderStage stage>
		void BindSRV(ID3D11DeviceContext* context, TextureID textureID, uint32_t slot)
		{
			const Texture& texture = GFX::Storage::GetTexture(textureID);
			ID3D11ShaderResourceView* const srv = texture.SRV.Get();
			ASSERT(srv, "BindSRV : Missing SRV");
			BindSRV<stage>(context, srv, slot);
		}

		template<ShaderStage stage>
		void BindSRV(ID3D11DeviceContext* context, BufferID bufferID, uint32_t slot)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			ID3D11ShaderResourceView* const srv = buffer.SRV.Get();
			ASSERT(srv, "BindSRV : Missing SRV");
			BindSRV<stage>(context, srv, slot);
		}

		template<ShaderStage stage>
		void BindUAV(ID3D11DeviceContext* context, BufferID bufferID, uint32_t slot)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			ID3D11UnorderedAccessView* const uav = buffer.UAV.Get();
			ASSERT(uav, "BindUAV : Missing UAV");
			BindUAV<stage>(context, uav, slot);
		}

		template<ShaderStage stage>
		void BindUAV(ID3D11DeviceContext* context, TextureID textureID, uint32_t slot)
		{
			const Texture& texture = GFX::Storage::GetTexture(textureID);
			ID3D11UnorderedAccessView* const uav = texture.UAV.Get();
			ASSERT(uav, "BindUAV : Missing UAV");
			BindUAV<stage>(context, uav, slot);
		}


	}
}