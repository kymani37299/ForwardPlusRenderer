#include "Commands.h"

#include "System/ApplicationConfiguration.h"
#include "Render/Device.h"
#include "Render/Resource.h"
#include "Render/Shader.h"
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
			if (!Device::Get()->IsMainContext(context)) return;

			if(!DebugMarkerHandle.Get()) API_CALL(context->QueryInterface(IID_PPV_ARGS(DebugMarkerHandle.GetAddressOf())));

			std::wstring wDebugName = StringUtility::ToWideString(markerName);
			DebugMarkerHandle->BeginEvent(wDebugName.c_str());
		}
		
		void MarkerEnd(ID3D11DeviceContext* context)
		{
			if (!Device::Get()->IsMainContext(context)) return;

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
			BindRenderTargets(context, { colorID }, depthID);
		}

		void BindRenderTargets(ID3D11DeviceContext* context, std::vector<TextureID> colorID, TextureID depthID)
		{
			ID3D11DepthStencilView* dsv = nullptr;
			std::vector<ID3D11RenderTargetView*> rtvs;
			rtvs.resize(colorID.size());
			for(uint32_t i=0;i<colorID.size();i++)
			{
				const TextureID texID = colorID[i];
				if (texID.Valid())
				{
					const Texture& colorTexture = GFX::Storage::GetTexture(texID);
					rtvs[i] = colorTexture.RTV.Get();
				}
				else
				{
					rtvs[i] = nullptr;
				}
			}
			if (depthID.Valid())
			{
				const Texture& depthTexture = GFX::Storage::GetTexture(depthID);
				dsv = depthTexture.DSV.Get();
			}
			context->OMSetRenderTargets(rtvs.size(), rtvs.size() == 0 ? nullptr : rtvs.data(), dsv);
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

		void ClearRenderTarget(ID3D11DeviceContext* context, TextureID id, Float4 clearColor)
		{
			const float clearValue[4] = { clearColor.x, clearColor.y, clearColor.z, clearColor.w };
			const Texture& tex = GFX::Storage::GetTexture(id);
			ASSERT(tex.RTV.Get(), "Trying to clear invalid color texture!");
			context->ClearRenderTargetView(tex.RTV.Get(), clearValue);
		}

		void ClearDepthStencil(ID3D11DeviceContext* context, TextureID id, float depthValue, uint32_t stencilValue)
		{
			const Texture& tex = GFX::Storage::GetTexture(id);
			ASSERT(tex.DSV.Get(), "Trying to clear invalid depth texture!");
			context->ClearDepthStencilView(tex.DSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depthValue, stencilValue);
		}

		void UploadToBuffer(ID3D11DeviceContext* context, BufferID bufferID, uint32_t dstOffset, const void* data, uint32_t srcOffset, size_t dataSize)
		{
			if (dataSize == 0) return;

			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
			if ((buffer.CreationFlags & RCF_CPU_Write) || (buffer.CreationFlags & RCF_CPU_Write_Persistent))
			{
				const D3D11_MAP mapMode = buffer.CreationFlags & RCF_CPU_Write_Persistent ? D3D11_MAP_WRITE : D3D11_MAP_WRITE_DISCARD;

				D3D11_MAPPED_SUBRESOURCE mapResult;
				API_CALL(context->Map(buffer.Handle.Get(), 0, mapMode, 0, &mapResult));

				const uint8_t* srcPtr = reinterpret_cast<const uint8_t*>(data);
				const uint8_t* dstPtr = reinterpret_cast<const uint8_t*>(mapResult.pData);

				memcpy((void*)(dstPtr + dstOffset), srcPtr + srcOffset, dataSize);
				context->Unmap(buffer.Handle.Get(), 0);
			}
			else if(buffer.CreationFlags & RCF_CopyDest)
			{
				const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);
				D3D11_BOX bufferRegion;
				bufferRegion.left = dstOffset;
				bufferRegion.right = dstOffset + dataSize;
				bufferRegion.top = 0;
				bufferRegion.bottom = 1;
				bufferRegion.front = 0;
				bufferRegion.back = 1;
				context->UpdateSubresource(buffer.Handle.Get(), 0, &bufferRegion, data, 0, 0);
			}
			else
			{
				ASSERT(0, "[UploadToBuffer, please set RCF_CopyDest or RCF_CPU_Write(_Persistent) for the resource you want to upload to!");
			}
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

		void CopyToBuffer(ID3D11DeviceContext* context, BufferID srcBuffer, uint32_t srcOffset, BufferID dstBuffer, uint32_t dstOffset, uint32_t size)
		{
			if (size == 0) return;

			const Buffer& srcBuf = GFX::Storage::GetBuffer(srcBuffer);
			const Buffer& dstBuf = GFX::Storage::GetBuffer(dstBuffer);
			D3D11_BOX sourceRegion;
			sourceRegion.left = srcOffset;
			sourceRegion.right = srcOffset + size;
			sourceRegion.top = 0;
			sourceRegion.bottom = 1;
			sourceRegion.front = 0;
			sourceRegion.back = 1;
			context->CopySubresourceRegion(dstBuf.Handle.Get(), 0, dstOffset, 0, 0, srcBuf.Handle.Get(), 0, &sourceRegion);
		}

		void ResolveTexture(ID3D11DeviceContext* context, TextureID srcTexture, TextureID dstTexture)
		{
			const Texture& srcTex = GFX::Storage::GetTexture(srcTexture);
			const Texture& dstTex = GFX::Storage::GetTexture(dstTexture);
			ASSERT(srcTex.Format == dstTex.Format, "In order to resolve a texture source and destination need to be a same format");
			context->ResolveSubresource(dstTex.Handle.Get(), 0, srcTex.Handle.Get(), 0, srcTex.Format);
		}
	}
}