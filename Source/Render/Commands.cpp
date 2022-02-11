#include "Commands.h"

#include "System/ApplicationConfiguration.h"
#include "Render/Resource.h"

namespace GFX
{
	namespace
	{
		DXGI_FORMAT IndexStrideToDXGIFormat(unsigned int indexStride)
		{
			switch (indexStride)
			{
			case 0: return DXGI_FORMAT_UNKNOWN;
			case 2: return DXGI_FORMAT_R16_UINT;
			case 4: return DXGI_FORMAT_R32_UINT;
			default: NOT_IMPLEMENTED;
			}
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	namespace Cmd
	{
		void ResetContext(ID3D11DeviceContext1* context)
		{
			context->ClearState();

			const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)AppConfig.WindowWidth, (float)AppConfig.WindowHeight, 0.0f, 1.0f };
			context->RSSetViewports(1, &viewport);
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			PipelineState defaultState = GFX::DefaultPipelineState();
			SetPipelineState(context, defaultState);
		}

		void BindShader(ID3D11DeviceContext1* context, ShaderID shaderID, bool multiInput)
		{
			const Shader& shader = GFX::Storage::GetShader(shaderID);
			context->VSSetShader(shader.VS.Get(), nullptr, 0);
			context->PSSetShader(shader.PS.Get(), nullptr, 0);
			if (multiInput)
				context->IASetInputLayout(shader.MIL.Get());
			else
				context->IASetInputLayout(shader.IL.Get());
		}

		void BindVertexBuffer(ID3D11DeviceContext1* context, BufferID bufferID)
		{
			BindVertexBuffers(context, { bufferID });
		}

		void BindVertexBuffers(ID3D11DeviceContext1* context, std::vector<BufferID> buffers)
		{
			std::vector<ID3D11Buffer*> bufferHandles;
			std::vector<uint32_t> offsets;
			std::vector<uint32_t> strides;

			for (BufferID id : buffers)
			{
				const Buffer& buffer = GFX::Storage::GetBuffer(id);

				bufferHandles.push_back(buffer.Handle.Get());
				offsets.push_back(0);
				strides.push_back(buffer.ElementStride);
			}

			context->IASetVertexBuffers(0, buffers.size(), bufferHandles.data(), strides.data(), offsets.data());
		}

		void BindIndexBuffer(ID3D11DeviceContext1* context, BufferID bufferID)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			context->IASetIndexBuffer(buffer.Handle.Get(), IndexStrideToDXGIFormat(buffer.ElementStride), 0);
		}

		void BindRenderTarget(ID3D11DeviceContext1* context, RenderTargetID rtID)
		{
			const Texture& colorTexture = GFX::Storage::GetTexture(rtID.ColorTexture);
			ID3D11DepthStencilView* dsv = nullptr;
			if (rtID.DepthTexture != TextureID_Invalid)
			{
				const Texture& depthTexture = GFX::Storage::GetTexture(rtID.DepthTexture);
				dsv = depthTexture.DSV.Get();
			}
			context->OMSetRenderTargets(1, colorTexture.RTV.GetAddressOf(), dsv);
		}

		void BindTextureSRV(ID3D11DeviceContext1* context, TextureID textureID, uint32_t slot)
		{
			const Texture& colorTex = GFX::Storage::GetTexture(textureID);
			ID3D11ShaderResourceView* srv = colorTex.SRV.Get();
			context->PSSetShaderResources(slot, 1, &srv);
		}

		void SetPipelineState(ID3D11DeviceContext1* context, const PipelineState& pipelineState)
		{
			const CompiledPipelineState& compiledState = GFX::CompilePipelineState(pipelineState);
			SetPipelineState(context, compiledState);
		}

		void SetPipelineState(ID3D11DeviceContext1* context, const CompiledPipelineState& pipelineState)
		{
			const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

			context->RSSetState(pipelineState.RS.Get());
			context->OMSetDepthStencilState(pipelineState.DS.Get(), 0xff);
			context->OMSetBlendState(pipelineState.BS.Get(), blendFactor, 0xff);
		}

		void UploadToBuffer(ID3D11DeviceContext1* context, BufferID bufferID, const void* data, uint64_t index)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			const uint8_t* dataBytePtr = reinterpret_cast<const uint8_t*>(data);

			D3D11_MAPPED_SUBRESOURCE mapResult;
			API_CALL(context->Map(buffer.Handle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResult));
			memcpy(mapResult.pData, dataBytePtr + (index * buffer.ElementStride), buffer.ElementStride);
			context->Unmap(buffer.Handle.Get(), 0);
		}
	}
}