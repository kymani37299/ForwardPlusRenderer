#include "Commands.h"

#include <WinPixEventRuntime/pix3.h>

#include "Render/Resource.h"
#include "Render/Buffer.h"
#include "Render/Device.h"
#include "Render/Texture.h"
#include "Render/Shader.h"

namespace GFX::Cmd
{
	GraphicsContext* CreateGraphicsContext()
	{
		GraphicsContext* context = new GraphicsContext{};

		D3D12_COMMAND_QUEUE_DESC queueDesc{};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		ID3D12Device* device = Device::Get()->GetHandle();
		API_CALL(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(context->CmdQueue.GetAddressOf())));
		API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(context->CmdAlloc.GetAddressOf())));
		API_CALL(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, context->CmdAlloc.Get(), nullptr /* Initial PSO */, IID_PPV_ARGS(context->CmdList.GetAddressOf())));
		API_CALL(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(context->CmdFence.Handle.GetAddressOf())));
		context->CmdFence.Value = 0;

		// TODO: Lower number of pages somehow
		context->MemContext.SRVHeap = DescriptorHeapGPU{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096u , 64u };
		context->MemContext.SRVPersistentPage = context->MemContext.SRVHeap.NewPage();

		return context;
	}

	void MarkerBegin(GraphicsContext& context, const std::string& name)
	{
		const uint64_t defaultColor = PIX_COLOR(0, 0, 0);
		PIXBeginEvent(context.CmdList.Get(), defaultColor, name.c_str());
	}

	void MarkerEnd(GraphicsContext& context)
	{
		PIXEndEvent(context.CmdList.Get());
	}

	void AddResourceTransition(std::vector<D3D12_RESOURCE_BARRIER>& barriers, Resource* res, D3D12_RESOURCE_STATES wantedState)
	{
		if (!res || res->CurrState & wantedState) return;

		if (res->Type == ResourceType::TextureSubresource)
		{
			TextureSubresource* subres = static_cast<TextureSubresource*>(res);
			for (uint32_t mip = subres->FirstMip; mip < subres->FirstMip + subres->MipCount; mip++)
			{
				for (uint32_t el = subres->FirstElement; el < subres->FirstElement + subres->ElementCount; el++)
				{
					D3D12_RESOURCE_BARRIER barrier;
					barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
					barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
					barrier.Transition.pResource = res->Handle.Get();
					barrier.Transition.StateBefore = res->CurrState;
					barrier.Transition.StateAfter = wantedState;
					barrier.Transition.Subresource = D3D12CalcSubresource(mip, el, 0, subres->NumMips, subres->NumElements);
					barriers.push_back(std::move(barrier));
				}
			}
		}
		else if (res->Type == ResourceType::BufferSubresource)
		{
			NOT_IMPLEMENTED;
		}
		else
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = res->Handle.Get();
			barrier.Transition.StateBefore = res->CurrState;
			barrier.Transition.StateAfter = wantedState;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barriers.push_back(std::move(barrier));
		}

		res->CurrState = wantedState;
	}

	void TransitionResource(GraphicsContext& context, Resource* resource, D3D12_RESOURCE_STATES wantedState)
	{
		std::vector<D3D12_RESOURCE_BARRIER> barriers{};
		AddResourceTransition(barriers, resource, wantedState);
		if(!barriers.empty()) context.CmdList->ResourceBarrier(barriers.size(), barriers.data());
	}

	void FlushContext(GraphicsContext& context)
	{
		Fence& fence = context.CmdFence;

		fence.Value++;
		API_CALL(context.CmdQueue->Signal(fence.Handle.Get(), fence.Value));
		if (context.CmdFence.Handle->GetCompletedValue() < context.CmdFence.Value)
		{
			void* eventHandle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
			if (eventHandle)
			{
				API_CALL(context.CmdFence.Handle->SetEventOnCompletion(context.CmdFence.Value, eventHandle));
				WaitForSingleObject(eventHandle, INFINITE);
				CloseHandle(eventHandle);
			}
			else
			{
				ASSERT(0, "[Fence::Sync] CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS) failed!");
			}
		}
	}

	void SubmitContext(GraphicsContext& context)
	{
		API_CALL(context.CmdList->Close());
		ID3D12CommandList* cmdsLists[] = { context.CmdList.Get() };
		context.CmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	}

	void ResetContext(GraphicsContext& context)
	{
		context.CmdAlloc->Reset();
		context.CmdList->Reset(context.CmdAlloc.Get(), nullptr);
		context.BoundState.Valid = false;
	}

	ID3D12CommandSignature* BindState(GraphicsContext& context, const GraphicsState& state)
	{
		return ApplyGraphicsState(context, state);
	}



	void UpdatePushConstants(GraphicsContext& context, const GraphicsState& state)
	{
		ASSERT(!state.PushConstants.empty(), "[UpdatePushConstants] Push constants are empty");

		const bool useCompute = state.Compute.pShaderBytecode != nullptr;
		if (useCompute) context.CmdList->SetComputeRoot32BitConstants(0, state.PushConstants.size(), state.PushConstants.data(), 0);
		else  context.CmdList->SetGraphicsRoot32BitConstants(0, state.PushConstants.size(), state.PushConstants.data(), 0);
	}

	void ClearRenderTarget(GraphicsContext& context, Texture* renderTarget)
	{
		TransitionResource(context, renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
		float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		D3D12_RECT rect = { 0.0f, 0.0f, renderTarget->Width, renderTarget->Height };
		context.CmdList->ClearRenderTargetView(renderTarget->RTV, clearColor, 1, &rect);
	}

	void ClearDepthStencil(GraphicsContext& context, Texture* depthStencil)
	{
		TransitionResource(context, depthStencil, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		D3D12_RECT rect = { 0.0f, 0.0f, depthStencil->Width, depthStencil->Height };
		context.CmdList->ClearDepthStencilView(depthStencil->DSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 1, &rect);
	}

	// TODO: Create staging resources directly on a Device and use that

	struct UpdateSubresourceInput
	{
		Resource* DstResource = nullptr;
		const void* Data = nullptr;
		uint32_t MipIndex = 0;
		uint32_t ArrayIndex = 0;
		uint32_t SrcOffset = 0;
		uint32_t DstOffset = 0;
		uint32_t DataSize = 0;
	};

	static void UpdateSubresource(GraphicsContext& context, UpdateSubresourceInput input)
	{
		Device* device = Device::Get();

		// Maybe use one staging buffer for everything	
		static constexpr uint32_t StagingOffset = 0;

		uint32_t subresourceIndex = 0;
		D3D12_SUBRESOURCE_DATA subresourceData{};
		subresourceData.pData = (const void*)(reinterpret_cast<const uint8_t*>(input.Data) + input.SrcOffset);
		if (input.DstResource->Type == ResourceType::Texture)
		{
			Texture* texture = static_cast<Texture*>(input.DstResource);
			subresourceIndex = D3D12CalcSubresource(input.MipIndex, input.ArrayIndex, 0, texture->NumMips, texture->NumElements);
			subresourceData.RowPitch = texture->RowPitch;
			subresourceData.SlicePitch = texture->SlicePitch;
		}
		else if (input.DstResource->Type == ResourceType::Buffer)
		{
			Buffer* buffer = static_cast<Buffer*>(input.DstResource);
			subresourceData.RowPitch = buffer->ByteSize;
			subresourceData.SlicePitch = subresourceData.RowPitch;
		}

		uint64_t resourceSize;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT subresLayout;
		uint32_t subresRowNumber;
		uint64_t subresRowByteSizes;

		ID3D12Resource* dxResource = input.DstResource->Handle.Get();
		D3D12_RESOURCE_DESC resourceDesc = dxResource->GetDesc();

		// Get memory footprints
		ID3D12Device* resourceDevice;
		dxResource->GetDevice(__uuidof(*resourceDevice), reinterpret_cast<void**>(&resourceDevice));
		resourceDevice->GetCopyableFootprints(&resourceDesc, subresourceIndex, 1, StagingOffset, &subresLayout, &subresRowNumber, &subresRowByteSizes, &resourceSize);
		resourceDevice->Release();

		// Create staging resource
		Buffer* stagingResource = GFX::CreateBuffer(input.DataSize ? input.DataSize : resourceSize, 1, RCF_CPU_Access);
		DeferredTrash::Put(stagingResource);
		GFX::SetDebugName(stagingResource, "UpdateSubresource::StagingBuffer");

		// Upload data to staging resource
		uint8_t* stagingDataPtr;
		API_CALL(stagingResource->Handle->Map(0, NULL, reinterpret_cast<void**>(&stagingDataPtr)));
		if (input.DataSize)
		{
			memcpy(stagingDataPtr, input.Data, input.DataSize);
		}
		else
		{
			D3D12_MEMCPY_DEST DestData = { stagingDataPtr + subresLayout.Offset, subresLayout.Footprint.RowPitch, (uint64_t)subresLayout.Footprint.RowPitch * subresRowNumber };
			MemcpySubresource(&DestData, &subresourceData, (uint32_t)subresRowByteSizes, subresRowNumber, subresLayout.Footprint.Depth);
		}
		stagingResource->Handle->Unmap(0, NULL);

		// Copy data to resource
		if (input.DstResource->Type == ResourceType::Buffer)
		{
			const uint32_t copySize = input.DataSize ? input.DataSize : subresLayout.Footprint.Width;
			Buffer* buffer = static_cast<Buffer*>(input.DstResource);
			TransitionResource(context, buffer, D3D12_RESOURCE_STATE_COPY_DEST);
			context.CmdList->CopyBufferRegion(buffer->Handle.Get(), input.DstOffset, stagingResource->Handle.Get(), subresLayout.Offset, copySize);
		}
		else
		{
			Texture* texture = static_cast<Texture*>(input.DstResource);
			TransitionResource(context, texture, D3D12_RESOURCE_STATE_COPY_DEST);

			D3D12_TEXTURE_COPY_LOCATION dst{};
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.pResource = texture->Handle.Get();
			dst.SubresourceIndex = subresourceIndex;

			D3D12_TEXTURE_COPY_LOCATION src{};
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.pResource = stagingResource->Handle.Get();
			src.PlacedFootprint = subresLayout;

			context.CmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		}

		DeferredTrash::Put(stagingResource->Handle.Get());
	}

	void UploadToBufferCPU(Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, size_t dataSize)
	{
		ASSERT(buffer->CreationFlags & RCF_CPU_Access, "[UploadToBufferCPU] Buffer must have CPU Access in order to map it");

		D3D12_RANGE mapRange{ 0, buffer->ByteSize };
		void* mappedData;
		API_CALL(buffer->Handle->Map(0, &mapRange, &mappedData));
		memcpy(mappedData, data, dataSize);
		buffer->Handle->Unmap(0, &mapRange);
	}

	void UploadToBufferGPU(GraphicsContext& context, Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, size_t dataSize)
	{
		UpdateSubresourceInput updateInput{};
		updateInput.DstResource = buffer;
		updateInput.Data = data;
		updateInput.DataSize = dataSize;
		updateInput.SrcOffset = srcOffset;
		updateInput.DstOffset = dstOffset;

		TransitionResource(context, buffer, D3D12_RESOURCE_STATE_COPY_DEST);
		UpdateSubresource(context, updateInput);
	}

	void UploadToBuffer(GraphicsContext& context, Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, size_t dataSize)
	{
		if (dataSize == 0) return;

		if (buffer->CreationFlags & RCF_CPU_Access)
		{
			UploadToBufferCPU(buffer, dstOffset, data, srcOffset, dataSize);
		}
		else
		{
			UploadToBufferGPU(context, buffer, dstOffset, data, srcOffset, dataSize);
		}
	}

	void UploadToTexture(GraphicsContext& context, const void* data, Texture* texture, uint32_t mipIndex, uint32_t arrayIndex)
	{
		UpdateSubresourceInput updateInput{};
		updateInput.DstResource = texture;
		updateInput.Data = data;
		updateInput.MipIndex = mipIndex;
		updateInput.ArrayIndex = arrayIndex;

		TransitionResource(context, texture, D3D12_RESOURCE_STATE_COPY_DEST);
		UpdateSubresource(context, updateInput);
	}

	void CopyToTexture(GraphicsContext& context, Texture* srcTexture, Texture* dstTexture, uint32_t mipIndex)
	{
		TransitionResource(context, srcTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
		TransitionResource(context, dstTexture, D3D12_RESOURCE_STATE_COPY_DEST);

		D3D12_TEXTURE_COPY_LOCATION srcCopy{};
		srcCopy.pResource = srcTexture->Handle.Get();
		srcCopy.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcCopy.SubresourceIndex = D3D12CalcSubresource(mipIndex, 0, 0, srcTexture->NumMips, srcTexture->NumElements);

		D3D12_TEXTURE_COPY_LOCATION dstCopy{};
		dstCopy.pResource = dstTexture->Handle.Get();
		srcCopy.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcCopy.SubresourceIndex = D3D12CalcSubresource(mipIndex, 0, 0, dstTexture->NumMips, dstTexture->NumElements);

		context.CmdList->CopyTextureRegion(&dstCopy, 0, 0, 0, &srcCopy, nullptr);
	}

	void CopyToBuffer(GraphicsContext& context, Buffer* srcBuffer, uint32_t srcOffset, Buffer* dstBuffer, uint32_t dstOffset, uint32_t size)
	{
		TransitionResource(context, srcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		TransitionResource(context, dstBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
		context.CmdList->CopyBufferRegion(dstBuffer->Handle.Get(), dstOffset, srcBuffer->Handle.Get(), srcOffset, size);
	}

	void DrawFC(GraphicsContext& context, GraphicsState& state)
	{
		state.VertexBuffers.resize(1);
		state.VertexBuffers[0] = Device::Get()->GetQuadBuffer();
		GFX::Cmd::BindState(context, state);
		context.CmdList->DrawInstanced(6, 1, 0, 0);
	}

	void BindShader(GraphicsState& state, Shader* shader, uint8_t stages, std::vector<std::string> config, bool multiInput)
	{
		const CompiledShader& compShader = GFX::GetCompiledShader(shader, config, stages);

		state.Pipeline.VS = compShader.Vertex;
		state.Pipeline.HS = compShader.Hull;
		state.Pipeline.DS = compShader.Domain;
		state.Pipeline.GS = compShader.Geometry;
		state.Pipeline.PS = compShader.Pixel;
		state.Compute = compShader.Compute;

		if (multiInput)
		{
			state.Pipeline.InputLayout = { compShader.InputLayoutMultiInput.data(), (uint32_t)compShader.InputLayoutMultiInput.size() };
		}
		else
		{
			state.Pipeline.InputLayout = { compShader.InputLayout.data(), (uint32_t)compShader.InputLayout.size() };
		}
	}

	void BindRenderTarget(GraphicsState& state, Texture* renderTarget)
	{
		if (renderTarget)
		{
			state.RenderTarget = renderTarget;
			state.Viewport = { 0.0f, 0.0f, (float)renderTarget->Width, (float)renderTarget->Height, 0.0f, 1.0f };
			state.Scissor = { 0,0, (long)renderTarget->Width, (long)renderTarget->Height };
			state.Pipeline.RTVFormats[0] = renderTarget->Format;
			state.Pipeline.NumRenderTargets = 1;
			state.Pipeline.SampleDesc.Count = GetSampleCount(renderTarget->CreationFlags);
		}
		else
		{
			state.RenderTarget = nullptr;
			state.Pipeline.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
			state.Pipeline.NumRenderTargets = 0;
		}

	}

	void BindDepthStencil(GraphicsState& state, Texture* depthStencil)
	{
		if (!depthStencil) NOT_IMPLEMENTED;

		state.DepthStencil = depthStencil;
		state.Viewport = { 0.0f, 0.0f, (float)depthStencil->Width, (float)depthStencil->Height, 0.0f, 1.0f };
		state.Scissor = { 0,0, (long)depthStencil->Width, (long)depthStencil->Height };
		state.Pipeline.DSVFormat = depthStencil->Format;
		state.Pipeline.SampleDesc.Count = GetSampleCount(depthStencil->CreationFlags);
	}

	void BindSampler(GraphicsState& state, uint32_t slot, D3D12_TEXTURE_ADDRESS_MODE addressMode, D3D12_FILTER filter)
	{
		D3D12_STATIC_SAMPLER_DESC samplerDesc{};
		samplerDesc.AddressU = addressMode;
		samplerDesc.AddressV = addressMode;
		samplerDesc.AddressW = addressMode;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.Filter = filter;
		samplerDesc.MaxAnisotropy = 16;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.MinLOD = 0;
		samplerDesc.MipLODBias = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderRegister = slot;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		state.Samplers.push_back(samplerDesc);
	}

	void GenerateMips(GraphicsContext& context, Texture* texture)
	{
		ASSERT(texture->NumElements == 1, "GenerateMips not supported for texture arrays!");

		GFX::Cmd::MarkerBegin(context, "GenerateMips");

		// Init state
		GraphicsState state{};
		state.Table.SRVs.resize(1);
		GFX::Cmd::BindSampler(state, 0, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
		GFX::Cmd::BindShader(state, Device::Get()->GetCopyShader(), VS | PS);

		// Generate subresources
		std::vector<TextureSubresource*> subresources{};
		subresources.resize(texture->NumMips);
		for (uint32_t mip = 0; mip < texture->NumMips; mip++)
		{
			subresources[mip] = GFX::CreateTextureSubresource(texture, mip, 1, 0, texture->NumElements);
			DeferredTrash::Put(subresources[mip]);
		}

		// Generate mips
		for (uint32_t mip = 1; mip < texture->NumMips; mip++)
		{
			state.Table.SRVs[0] = subresources[mip - 1];
			GFX::Cmd::BindRenderTarget(state, subresources[mip]);
			GFX::Cmd::DrawFC(context, state);
		}

		// Return resource state to initial state
		for (uint32_t mip = 0; mip < texture->NumMips; mip++)
		{
			GFX::Cmd::TransitionResource(context, subresources[mip], texture->CurrState);
		}

		GFX::Cmd::MarkerEnd(context);
	}

	void ResolveTexture(GraphicsContext& context, Texture* inputTexture, Texture* outputTexture)
	{
		TransitionResource(context, inputTexture, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		TransitionResource(context, outputTexture, D3D12_RESOURCE_STATE_RESOLVE_DEST);
		context.CmdList->ResolveSubresource(outputTexture->Handle.Get(), 0, inputTexture->Handle.Get(), 0, outputTexture->Format);
	}

}