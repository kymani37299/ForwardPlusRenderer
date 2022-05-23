#pragma once

#include <vector>

#include "Render/RenderAPI.h"
#include "Render/ResourceID.h"
#include "Render/Resource.h"
#include "Render/Texture.h"
#include "Render/Shader.h"
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

		void BindVertexBuffer(ID3D11DeviceContext* context, BufferID bufferID);
		void BindVertexBuffers(ID3D11DeviceContext* context, std::vector<BufferID> buffers);
		void BindIndexBuffer(ID3D11DeviceContext* context, BufferID bufferID);
		void BindRenderTarget(ID3D11DeviceContext* context, TextureID colorID, TextureID depthID = TextureID{});
		void BindRenderTargets(ID3D11DeviceContext* context, std::vector<TextureID> colorID, TextureID depthID = TextureID{});

		void SetViewport(ID3D11DeviceContext* context, Float2 viewportSize);
		void SetPipelineState(ID3D11DeviceContext* context, const PipelineState& pipelineState);
		void SetPipelineState(ID3D11DeviceContext* context, const CompiledPipelineState& pipelineState);

		void ClearRenderTarget(ID3D11DeviceContext* context, TextureID id, Float4 clearColor = { 0.0f, 0.0f, 0.0f, 0.0f });
		void ClearDepthStencil(ID3D11DeviceContext* context, TextureID id, float depthValue = 1.0f, uint32_t stencilValue = 0);
		void UploadToBuffer(ID3D11DeviceContext* context, BufferID bufferID, uint32_t dstOffset, const void* data, uint32_t srcOffset, size_t dataSize);
		void UploadToTexture(ID3D11DeviceContext* context, void* data, TextureID textureID, uint32_t mipIndex = 0);
		void CopyToTexture(ID3D11DeviceContext* context, TextureID srcTexture, TextureID dstTexture, uint32_t mipIndex = 0);
		void CopyToBuffer(ID3D11DeviceContext* context, BufferID srcBuffer, uint32_t srcOffset, BufferID dstBuffer, uint32_t dstOffset, uint32_t size);

		void ResolveTexture(ID3D11DeviceContext* context, TextureID srcTexture, TextureID dstTexture);

		template<uint32_t stage>
		void SetupStaticSamplers(ID3D11DeviceContext* context)
		{
			if (stage & VS) context->VSSetSamplers(0, GFX::GetStaticSamplersNum(), GFX::GetStaticSamplers());
			if (stage & PS) context->PSSetSamplers(0, GFX::GetStaticSamplersNum(), GFX::GetStaticSamplers());
			if (stage & CS) context->CSSetSamplers(0, GFX::GetStaticSamplersNum(), GFX::GetStaticSamplers());
		}

		template<uint32_t stage>
		void BindShader(ID3D11DeviceContext* context, ShaderID shaderID, std::vector<std::string> configuration = {}, bool multiInput = false)
		{
			const ShaderImplementation& impl = GFX::GetShaderImplementation(shaderID, configuration, stage);

			context->VSSetShader(impl.VS.Get(), nullptr, 0);
			context->PSSetShader(impl.PS.Get(), nullptr, 0);
			context->CSSetShader(impl.CS.Get(), nullptr, 0);

			if (multiInput)
				context->IASetInputLayout(impl.MIL.Get());
			else
				context->IASetInputLayout(impl.IL.Get());
		}

		template<uint32_t stage>
		void BindCBV(ID3D11DeviceContext* context, ID3D11Buffer* cbv, uint32_t slot)
		{
			if (stage & VS) context->VSSetConstantBuffers(slot, 1, &cbv);
			if (stage & PS) context->PSSetConstantBuffers(slot, 1, &cbv);
			if (stage & CS) context->CSSetConstantBuffers(slot, 1, &cbv);
		}

		template<uint32_t stage>
		void BindSRV(ID3D11DeviceContext* context, ID3D11ShaderResourceView* srv, uint32_t slot)
		{
			if (stage & VS) context->VSSetShaderResources(slot, 1, &srv);
			if (stage & PS) context->PSSetShaderResources(slot, 1, &srv);
			if (stage & CS) context->CSSetShaderResources(slot, 1, &srv);
		}

		template<uint32_t stage>
		void BindUAV(ID3D11DeviceContext* context, ID3D11UnorderedAccessView* uav, uint32_t slot)
		{
			ASSERT(stage & CS, "UAV are not supported by other stages except CS");
;			context->CSSetUnorderedAccessViews(slot, 1, &uav, nullptr);
		}

		template<uint32_t stage>
		void BindCBV(ID3D11DeviceContext* context, BufferID bufferID, uint32_t slot)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			ID3D11Buffer* const cbv = buffer.Handle.Get();
			ASSERT(cbv, "BindCBV : Missing CBV");
			BindCBV<stage>(context, cbv, slot);
		}

		template<uint32_t stage>
		void BindSRV(ID3D11DeviceContext* context, TextureID textureID, uint32_t slot)
		{
			const Texture& texture = GFX::Storage::GetTexture(textureID);
			ID3D11ShaderResourceView* const srv = texture.SRV.Get();
			ASSERT(srv, "BindSRV : Missing SRV");
			BindSRV<stage>(context, srv, slot);
		}

		template<uint32_t stage>
		void BindSRV(ID3D11DeviceContext* context, BufferID bufferID, uint32_t slot)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			ID3D11ShaderResourceView* const srv = buffer.SRV.Get();
			ASSERT(srv, "BindSRV : Missing SRV");
			BindSRV<stage>(context, srv, slot);
		}

		template<uint32_t stage>
		void BindUAV(ID3D11DeviceContext* context, BufferID bufferID, uint32_t slot)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			ID3D11UnorderedAccessView* const uav = buffer.UAV.Get();
			ASSERT(uav, "BindUAV : Missing UAV");
			BindUAV<stage>(context, uav, slot);
		}

		template<uint32_t stage>
		void BindUAV(ID3D11DeviceContext* context, TextureID textureID, uint32_t slot)
		{
			const Texture& texture = GFX::Storage::GetTexture(textureID);
			ID3D11UnorderedAccessView* const uav = texture.UAV.Get();
			ASSERT(uav, "BindUAV : Missing UAV");
			BindUAV<stage>(context, uav, slot);
		}
	}
}