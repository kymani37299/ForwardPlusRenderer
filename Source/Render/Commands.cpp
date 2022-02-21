#include "Commands.h"

#include "System/ApplicationConfiguration.h"
#include "Render/Device.h"
#include "Render/Resource.h"
#include "Utility/StringUtility.h"

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
		static ComPtr<ID3DUserDefinedAnnotation> DebugMarkerHandle;

		void MarkerBegin(ID3D11DeviceContext* context, const std::string& markerName)
		{
			if(!DebugMarkerHandle.Get()) API_CALL(context->QueryInterface(IID_PPV_ARGS(DebugMarkerHandle.GetAddressOf())));

			std::wstring wDebugName = StringUtility::ToWideString(markerName);
			DebugMarkerHandle->BeginEvent(wDebugName.c_str());
		}
		
		void MarkerEnd(ID3D11DeviceContext* context)
		{
			ASSERT(DebugMarkerHandle, "[Cmd::MarkerEnd] Called MarkerEnd without MarkerBegin before it!");
			DebugMarkerHandle->EndEvent();
		}

		void ResetContext(ID3D11DeviceContext* context)
		{
			context->ClearState();

			const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)AppConfig.WindowWidth, (float)AppConfig.WindowHeight, 0.0f, 1.0f };
			context->RSSetViewports(1, &viewport);
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			PipelineState defaultState = GFX::DefaultPipelineState();
			SetPipelineState(context, defaultState);
		}

		ID3D11DeviceContext* CreateDeferredContext()
		{
			ID3D11DeviceContext* context;
			API_CALL(Device::Get()->GetHandle()->CreateDeferredContext(0, &context));
			return context;
		}

		void SubmitDeferredContext(ID3D11DeviceContext* context)
		{
			Device::Get()->SubmitDeferredContext(context);
		}

		void BindShader(ID3D11DeviceContext* context, ShaderID shaderID, bool multiInput)
		{
			const Shader& shader = GFX::Storage::GetShader(shaderID);
			context->VSSetShader(shader.VS.Get(), nullptr, 0);
			context->PSSetShader(shader.PS.Get(), nullptr, 0);
			if (multiInput)
				context->IASetInputLayout(shader.MIL.Get());
			else
				context->IASetInputLayout(shader.IL.Get());
		}

		void BindVertexBuffer(ID3D11DeviceContext* context, BufferID bufferID)
		{
			BindVertexBuffers(context, { bufferID });
		}

		void BindVertexBuffers(ID3D11DeviceContext* context, std::vector<BufferID> buffers)
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

		void BindIndexBuffer(ID3D11DeviceContext* context, BufferID bufferID)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			context->IASetIndexBuffer(buffer.Handle.Get(), IndexStrideToDXGIFormat(buffer.ElementStride), 0u);
		}

		void BindRenderTarget(ID3D11DeviceContext* context, TextureID colorID, TextureID depthID)
		{
			ID3D11RenderTargetView* rtv = nullptr;
			ID3D11DepthStencilView* dsv = nullptr;
			if (colorID.Valid())
			{
				const Texture& colorTexture = GFX::Storage::GetTexture(colorID);
				rtv = colorTexture.RTV.Get();
			}
			if (depthID.Valid())
			{
				const Texture& depthTexture = GFX::Storage::GetTexture(depthID);
				dsv = depthTexture.DSV.Get();
			}
			context->OMSetRenderTargets(1, &rtv, dsv);
		}

		void BindCBV_VS(ID3D11DeviceContext* context, BufferID bufferID, uint32_t slot)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			ID3D11Buffer* const cbv = buffer.Handle.Get();
			context->VSSetConstantBuffers(slot, 1, &cbv);
		}

		void BindSRV_PS(ID3D11DeviceContext* context, TextureID textureID, uint32_t slot)
		{
			const Texture& texture = GFX::Storage::GetTexture(textureID);
			ID3D11ShaderResourceView *const srv = texture.SRV.Get();
			context->PSSetShaderResources(slot, 1, &srv);
		}

		void SetViewport(ID3D11DeviceContext* context, Float2 viewportSize)
		{
			const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)viewportSize.x, (float)viewportSize.y, 0.0f, 1.0f };
			context->RSSetViewports(1, &viewport);
		}

		void SetPipelineState(ID3D11DeviceContext* context, const PipelineState& pipelineState)
		{
			const CompiledPipelineState& compiledState = GFX::CompilePipelineState(pipelineState);
			SetPipelineState(context, compiledState);
		}

		void SetPipelineState(ID3D11DeviceContext* context, const CompiledPipelineState& pipelineState)
		{
			const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

			context->RSSetState(pipelineState.RS.Get());
			context->OMSetDepthStencilState(pipelineState.DS.Get(), 0xff);
			context->OMSetBlendState(pipelineState.BS.Get(), blendFactor, 0xffffffff);
		}

		void ClearRenderTarget(ID3D11DeviceContext* context, TextureID colorID, TextureID depthID)
		{
			if (colorID.Valid())
			{
				const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				const Texture& colorTexture = GFX::Storage::GetTexture(colorID);
				context->ClearRenderTargetView(colorTexture.RTV.Get(), clearColor);
			}
			if (depthID.Valid())
			{
				const Texture& depthTexture = GFX::Storage::GetTexture(depthID);
				context->ClearDepthStencilView(depthTexture.DSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			}
		}

		void UploadToBuffer(ID3D11DeviceContext* context, BufferID bufferID, const void* data, size_t dataSize, size_t offset)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			const uint8_t* dataBytePtr = reinterpret_cast<const uint8_t*>(data);

			D3D11_MAPPED_SUBRESOURCE mapResult;
			API_CALL(context->Map(buffer.Handle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResult));
			memcpy(mapResult.pData, dataBytePtr + offset, dataSize);
			context->Unmap(buffer.Handle.Get(), 0);
		}

		void UploadToTexture(ID3D11DeviceContext* context, void* data, TextureID textureID, uint32_t mipIndex)
		{
			const Texture& texture = GFX::Storage::GetTexture(textureID);
			uint32_t subresourceIndex = D3D11CalcSubresource(mipIndex, 0, texture.NumMips);
			context->UpdateSubresource(texture.Handle.Get(), subresourceIndex, nullptr, data, texture.RowPitch, texture.SlicePitch);
		}

		void CopyToTexture(ID3D11DeviceContext* context, TextureID srcTexture, TextureID dstTexture, uint32_t mipIndex)
		{
			const Texture& srcTex = GFX::Storage::GetTexture(srcTexture);
			const Texture& dstTex = GFX::Storage::GetTexture(dstTexture);
			ASSERT(srcTex.RowPitch == dstTex.RowPitch
				&& srcTex.SlicePitch == dstTex.SlicePitch
				&& srcTex.NumMips == dstTex.NumMips, "[Cmd::CopyToTexture] SRC and DST are not compatible!");
			uint32_t subresourceIndex = D3D11CalcSubresource(mipIndex, 0, srcTex.NumMips);
			context->CopySubresourceRegion(dstTex.Handle.Get(), subresourceIndex, 0, 0, 0, srcTex.Handle.Get(), subresourceIndex, nullptr);
		}
	}
}