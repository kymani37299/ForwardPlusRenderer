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

		context->MemContext.SRVHeap = DescriptorHeapGPU{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 64u * 1024, 256u * 1024u };
		context->MemContext.SMPHeap = DescriptorHeapGPU{ D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256u,  1024u };

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
					barrier.Transition.Subresource = GFX::GetSubresourceIndex(subres, mip, el);
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
		if(!barriers.empty()) context.CmdList->ResourceBarrier((UINT) barriers.size(), barriers.data());
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

	void SetPushConstants(uint32_t shaderStages, GraphicsContext& context, const BindVector<uint32_t> values)
	{
		ASSERT(!values.empty(), "[UpdatePushConstants] Push constants are empty");

		const bool useCompute = shaderStages & CS;
		if (useCompute) context.CmdList->SetComputeRoot32BitConstants(0, (UINT)values.size(), values.data(), 0);
		else  context.CmdList->SetGraphicsRoot32BitConstants(0, (UINT)values.size(), values.data(), 0);
	}

	void ClearRenderTarget(GraphicsContext& context, Texture* renderTarget)
	{
		TransitionResource(context, renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
		float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		D3D12_RECT rect = { 0, 0, (long) renderTarget->Width, (long) renderTarget->Height };
		context.CmdList->ClearRenderTargetView(renderTarget->RTV, clearColor, 1, &rect);
	}

	void ClearDepthStencil(GraphicsContext& context, Texture* depthStencil)
	{
		TransitionResource(context, depthStencil, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		D3D12_RECT rect = { 0, 0, (long) depthStencil->Width, (long) depthStencil->Height };
		context.CmdList->ClearDepthStencilView(depthStencil->DSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 1, &rect);
	}

	// TODO: Create staging resources directly on a Device and use that

	void UploadToBufferCPU(Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, uint32_t dataSize)
	{
		ASSERT(buffer->CreationFlags & RCF_CPU_Access, "[UploadToBufferCPU] Buffer must have CPU Access in order to map it");

		D3D12_RANGE mapRange{ 0, buffer->ByteSize };
		void* mappedData;
		API_CALL(buffer->Handle->Map(0, &mapRange, &mappedData));
		memcpy(mappedData, data, dataSize);
		buffer->Handle->Unmap(0, &mapRange);
	}

	void UploadToBufferGPU(GraphicsContext& context, Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, uint32_t dataSize)
	{
		// Create staging resource
		Buffer* stagingResource = GFX::CreateBuffer(dataSize, 1, RCF_CPU_Access | RCF_No_SRV);
		DeferredTrash::Get()->Put(stagingResource);
		GFX::SetDebugName(stagingResource, "UpdateSubresource::StagingBuffer");

		// Upload data to staging resource
		uint8_t* stagingDataPtr;
		uint8_t* srcDataPtr = (uint8_t*) data;
		API_CALL(stagingResource->Handle->Map(0, NULL, reinterpret_cast<void**>(&stagingDataPtr)));
		memcpy(stagingDataPtr, srcDataPtr + srcOffset, dataSize);
		stagingResource->Handle->Unmap(0, NULL);

		// Copy to buffer
		const uint32_t copySize = dataSize;
		TransitionResource(context, buffer, D3D12_RESOURCE_STATE_COPY_DEST);
		context.CmdList->CopyBufferRegion(buffer->Handle.Get(), dstOffset, stagingResource->Handle.Get(), 0, copySize);
	}

	void UploadToBuffer(GraphicsContext& context, Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, uint32_t dataSize)
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
		Device* device = Device::Get();

		// Get memory footprints
		uint32_t subresourceIndex = 0;
		D3D12_SUBRESOURCE_DATA subresourceData{};
		subresourceData.pData = data;
		subresourceIndex = GFX::GetSubresourceIndex(texture, mipIndex, arrayIndex);
		subresourceData.RowPitch = texture->RowPitch;
		subresourceData.SlicePitch = texture->SlicePitch;

		uint64_t resourceSize;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT subresLayout;
		uint32_t subresRowNumber;
		uint64_t subresRowByteSizes;

		ID3D12Resource* dxResource = texture->Handle.Get();
		D3D12_RESOURCE_DESC resourceDesc = dxResource->GetDesc();
		ID3D12Device* resourceDevice;
		dxResource->GetDevice(__uuidof(*resourceDevice), reinterpret_cast<void**>(&resourceDevice));
		resourceDevice->GetCopyableFootprints(&resourceDesc, subresourceIndex, 1, 0, &subresLayout, &subresRowNumber, &subresRowByteSizes, &resourceSize);
		resourceDevice->Release();

		// Create staging resource
		Buffer* stagingResource = GFX::CreateBuffer((uint32_t)resourceSize, 1, RCF_CPU_Access | RCF_No_SRV);
		DeferredTrash::Get()->Put(stagingResource);
		GFX::SetDebugName(stagingResource, "UpdateSubresource::StagingBuffer");

		// Upload data to staging resource
		uint8_t* stagingDataPtr;
		API_CALL(stagingResource->Handle->Map(0, NULL, reinterpret_cast<void**>(&stagingDataPtr)));
		D3D12_MEMCPY_DEST DestData = { stagingDataPtr + subresLayout.Offset, subresLayout.Footprint.RowPitch, (uint64_t)subresLayout.Footprint.RowPitch * subresRowNumber };
		MemcpySubresource(&DestData, &subresourceData, (uint32_t)subresRowByteSizes, subresRowNumber, subresLayout.Footprint.Depth);
		stagingResource->Handle->Unmap(0, NULL);

		// Copy to texture
		D3D12_TEXTURE_COPY_LOCATION dst{};
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.pResource = texture->Handle.Get();
		dst.SubresourceIndex = subresourceIndex;

		D3D12_TEXTURE_COPY_LOCATION src{};
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.pResource = stagingResource->Handle.Get();
		src.PlacedFootprint = subresLayout;

		TransitionResource(context, texture, D3D12_RESOURCE_STATE_COPY_DEST);
		context.CmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}

	void CopyToTexture(GraphicsContext& context, Texture* srcTexture, Texture* dstTexture, uint32_t mipIndex)
	{
		TransitionResource(context, srcTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
		TransitionResource(context, dstTexture, D3D12_RESOURCE_STATE_COPY_DEST);

		D3D12_TEXTURE_COPY_LOCATION srcCopy{};
		srcCopy.pResource = srcTexture->Handle.Get();
		srcCopy.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcCopy.SubresourceIndex = GFX::GetSubresourceIndex(srcTexture, mipIndex, 0);

		D3D12_TEXTURE_COPY_LOCATION dstCopy{};
		dstCopy.pResource = dstTexture->Handle.Get();
		dstCopy.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstCopy.SubresourceIndex = GFX::GetSubresourceIndex(dstTexture, mipIndex, 0);

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
		state.VertexBuffers[0] = Device::Get()->GetQuadBuffer();
		context.ApplyState(state);
		context.CmdList->DrawInstanced(6, 1, 0, 0);
	}

	void GenerateMips(GraphicsContext& context, Texture* texture)
	{
		ASSERT(texture->DepthOrArraySize == 1, "GenerateMips not supported for texture arrays!");
		
		GFX::Cmd::MarkerBegin(context, "GenerateMips");

		// Get staging texture
		StagingResourcesContext::StagingTextureRequest texRequest = {};
		texRequest.Width = texture->Width;
		texRequest.Height = texture->Height;
		texRequest.NumMips = texture->NumMips;
		texRequest.Format = texture->Format;
		texRequest.CreationFlags = RCF_Bind_RTV | RCF_GenerateMips;
		texRequest.CreateSubresources = true;
		StagingResourcesContext::StagingTexture* stagingTexture = context.StagingResources.GetTransientTexture(texRequest);

		// Copy data to staging texture
		GFX::Cmd::CopyToTexture(context, texture, stagingTexture->TextureResource, 0);

		// Init state
		GraphicsState state{};
		state.Shader = Device::Get()->GetCopyShader();
		state.Table.SMPs[0] = Sampler{ D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
		
		// Generate mips
		for (uint32_t mip = 1; mip < texture->NumMips; mip++)
		{
			state.Table.SRVs[0] = stagingTexture->Subresources[mip - 1];
			state.RenderTargets[0] = stagingTexture->Subresources[mip];
			GFX::Cmd::DrawFC(context, state);
		}

		// Copy to target texture
		for (uint32_t mip = 0; mip < texture->NumMips; mip++) 
			GFX::Cmd::CopyToTexture(context, stagingTexture->TextureResource, texture, mip);

		// Return resource state to initial state
		for (uint32_t mip = 0; mip < texture->NumMips; mip++)
			GFX::Cmd::TransitionResource(context, stagingTexture->Subresources[mip], stagingTexture->TextureResource->CurrState);

		GFX::Cmd::MarkerEnd(context);
	}

	void ResolveTexture(GraphicsContext& context, Texture* inputTexture, Texture* outputTexture)
	{
		TransitionResource(context, inputTexture, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		TransitionResource(context, outputTexture, D3D12_RESOURCE_STATE_RESOLVE_DEST);
		context.CmdList->ResolveSubresource(outputTexture->Handle.Get(), 0, inputTexture->Handle.Get(), 0, outputTexture->Format);
	}

	template<typename T>
	BindlessTable CreateBindlessTable(GraphicsContext& context, std::vector<T*> resources, uint32_t registerSpace)
	{
		ASSERT(registerSpace > 0u, "[CreateBindlessTable] Bindless resources must exist on space that is not 0");
		ASSERT(!resources.empty(), "[CreateBindlessTable] Cannot create bindless table with no resources!");

		DescriptorHeapGPU& heap = context.MemContext.SRVHeap;

		BindlessTable table;
		table.DescriptorTable = heap.Allocate(resources.size());
		table.RegisterSpace = registerSpace;
		table.DescriptorCount = (uint32_t) resources.size();

		// Fill descriptor table
		for (uint32_t i = 0; i < resources.size(); i++)
		{
			const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor = resources[i]->SRV;
			const D3D12_CPU_DESCRIPTOR_HANDLE dstDescriptor = heap.GetCPUHandle(table.DescriptorTable, i);
			Device::Get()->GetHandle()->CopyDescriptorsSimple(1, dstDescriptor, srcDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		
		return table;
	}

	BindlessTable CreateBindlessTable(GraphicsContext& context, std::vector<Texture*> textures, uint32_t registerSpace) { return CreateBindlessTable<Texture>(context, textures, registerSpace); }
	BindlessTable CreateBindlessTable(GraphicsContext& context, std::vector<Buffer*> buffers, uint32_t registerSpace) { return CreateBindlessTable<Buffer>(context, buffers, registerSpace); }

	void ReleaseBindlessTable(GraphicsContext& context, const BindlessTable& table)
	{
		if(table.DescriptorTable.HeapAlloc.NumElements != INVALID_ALLOCATION)
			DeferredTrash::Get()->Put(&context.MemContext.SRVHeap, table.DescriptorTable);
	}

}