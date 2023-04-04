#include "Commands.h"

#include <WinPixEventRuntime/pix3.h>

#include "Render/Resource.h"
#include "Render/Buffer.h"
#include "Render/Device.h"
#include "Render/Texture.h"
#include "Render/Shader.h"
#include "Render/RenderResources.h"

#define PROFILE_GFX_CMD
#ifdef PROFILE_GFX_CMD
#define PROFILE_CMD() PROFILE_FUNCTION()
#else
#define PROFILE_CMD()
#endif

namespace GFX::Cmd
{
	void MarkerBegin(const GraphicsContext& context, const std::string& name)
	{
		const uint64_t defaultColor = PIX_COLOR(0, 0, 0);
		PIXBeginEvent(context.CmdList.Get(), defaultColor, name.c_str());
	}

	void MarkerEnd(const GraphicsContext& context)
	{
		PIXEndEvent(context.CmdList.Get());
	}

	void AddResourceTransition(std::vector<D3D12_RESOURCE_BARRIER>& barriers, Resource* res, D3D12_RESOURCE_STATES wantedState)
	{
		if (!res) return;

		PROFILE_CMD();


		const bool needsUAVBarrier = (res->CurrState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) && (wantedState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		if ((res->CurrState & wantedState) && !needsUAVBarrier) 
			return;

		if (needsUAVBarrier)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.UAV.pResource = res->Handle.Get();
			barriers.push_back(std::move(barrier));
		}
		else
		{
			if (res->Type == ResourceType::TextureSubresource)
			{
				TextureSubresourceView* subres = static_cast<TextureSubresourceView*>(res);
				for (uint32_t mip = subres->FirstMip; mip <= subres->LastMip; mip++)
				{
					for (uint32_t el = subres->FirstElement; el <= subres->LastElement; el++)
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
	}

	void TransitionResource(GraphicsContext& context, Resource* resource, D3D12_RESOURCE_STATES wantedState)
	{
		std::vector<D3D12_RESOURCE_BARRIER> barriers{};
		AddResourceTransition(barriers, resource, wantedState);
		if(!barriers.empty()) context.CmdList->ResourceBarrier((UINT) barriers.size(), barriers.data());
	}

	void WaitToFinish(GraphicsContext& context)
	{
		PROFILE_CMD();

		Fence& fence = context.CmdFence;
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

	void BeginRecording(GraphicsContext& context)
	{
		// For some reason this messes up the capture
		// PROFILE_CMD();

		ASSERT(context.Closed, "Trying to begin recording on open context!");
		
		WaitToFinish(context);

		// Clear inframe resources
		{
			MemoryContext& mem = context.MemContext;

			for (DescriptorAllocation descriptorAlloc : mem.FrameDescriptors) descriptorAlloc.Release();
			for (Shader* shader : mem.FrameShaders) delete shader;
			for (Resource* resource : mem.FrameResources) delete resource;

			mem.FrameDXResources.clear();
			mem.FrameDescriptors.clear();
			mem.FrameShaders.clear();
			mem.FrameResources.clear();
		}

		// Sync readback buffers
		{
			for (ReadbackBuffer* readbackBuffer : context.PendingReadbacks)
			{
				readbackBuffer->Private_Sync();

			}
			context.PendingReadbacks.clear();
		}

		// Clear api state
		{
			context.CmdAlloc->Reset();
			context.CmdList->Reset(context.CmdAlloc.Get(), nullptr);
			context.BoundState.Valid = false;
		}

		context.Closed = false;
	}

	void EndRecordingAndSubmit(GraphicsContext& context)
	{
		PROFILE_CMD();

		ASSERT(!context.Closed, "Trying to submit closed context!");

		API_CALL(context.CmdList->Close());
		context.Closed = true;

		ID3D12CommandList* cmdsLists[] = { context.CmdList.Get() };
		Device::Get()->GetCommandQueue()->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		Fence& fence = context.CmdFence;
		fence.Value++;
		API_CALL(Device::Get()->GetCommandQueue()->Signal(fence.Handle.Get(), fence.Value));
	}

	void SetPushConstants(uint32_t shaderStages, GraphicsContext& context, const PushConstantTable& values)
	{
		PROFILE_CMD();

		ASSERT(!values.empty(), "[UpdatePushConstants] Push constants are empty");

		const bool useCompute = shaderStages & CS;
		if (useCompute) context.CmdList->SetComputeRoot32BitConstants(0, (UINT)values.size(), values.data(), 0);
		else  context.CmdList->SetGraphicsRoot32BitConstants(0, (UINT)values.size(), values.data(), 0);
	}

	void ClearRenderTarget(GraphicsContext& context, Texture* renderTarget)
	{
		PROFILE_CMD();

		TransitionResource(context, renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
		float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		D3D12_RECT rect = { 0, 0, (long) renderTarget->Width, (long) renderTarget->Height };
		context.CmdList->ClearRenderTargetView(renderTarget->RTV.GetCPUHandle(), clearColor, 1, &rect);
	}

	void ClearDepthStencil(GraphicsContext& context, Texture* depthStencil)
	{
		PROFILE_CMD();

		TransitionResource(context, depthStencil, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		D3D12_RECT rect = { 0, 0, (long) depthStencil->Width, (long) depthStencil->Height };
		context.CmdList->ClearDepthStencilView(depthStencil->DSV.GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 1, &rect);
	}

	void UploadToBufferImmediate(Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, uint32_t dataSize)
	{
		PROFILE_CMD();

		ASSERT(TestFlag(buffer->CreationFlags, RCF::CPU_Access), "[UploadToBufferImmediate] Buffer must have CPU Access for immediate upload");

		// TODO: Map only required part
		void* mappedData;
		D3D12_RANGE mapRange{ 0, buffer->ByteSize };
		API_CALL(buffer->Handle->Map(0, &mapRange, &mappedData));

		uint8_t* srcData = (uint8_t*)data;
		uint8_t* dstData = (uint8_t*)mappedData;
		memcpy(dstData + dstOffset, srcData + srcOffset, dataSize);

		buffer->Handle->Unmap(0, &mapRange);
	}

	void UploadToBuffer(GraphicsContext& context, Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, uint32_t dataSize)
	{
		if (dataSize == 0) return;

		PROFILE_CMD();

		// Create staging resource
		Buffer* stagingResource = GFX::CreateBuffer(dataSize, 1, RCF::CPU_Access | RCF::NoSRV);
		GFX::SetDebugName(stagingResource, "UpdateSubresource::StagingBuffer");

		// Upload data to staging resource
		GFX::Cmd::UploadToBufferImmediate(stagingResource, 0, data, srcOffset, dataSize);
		
		// Copy to buffer
		const uint32_t copySize = dataSize;
		TransitionResource(context, buffer, D3D12_RESOURCE_STATE_COPY_DEST);
		context.CmdList->CopyBufferRegion(buffer->Handle.Get(), dstOffset, stagingResource->Handle.Get(), 0, copySize);
		
		GFX::Cmd::Delete(context, stagingResource);
	}

	void UploadToTexture(GraphicsContext& context, const void* data, Texture* texture, uint32_t mipIndex, uint32_t arrayIndex)
	{
		PROFILE_CMD();

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
		Buffer* stagingResource = GFX::CreateBuffer((uint32_t)resourceSize, 1, RCF::CPU_Access | RCF::NoSRV);
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

		GFX::Cmd::Delete(context, stagingResource);
	}

	void CopyToTexture(GraphicsContext& context, Texture* srcTexture, Texture* dstTexture, uint32_t mipIndex)
	{
		PROFILE_CMD();

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
		PROFILE_CMD();

		TransitionResource(context, srcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		TransitionResource(context, dstBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
		context.CmdList->CopyBufferRegion(dstBuffer->Handle.Get(), dstOffset, srcBuffer->Handle.Get(), srcOffset, size);
	}

	void ClearBuffer(GraphicsContext& context, Buffer* buffer)
	{
		PROFILE_CMD();

		std::vector<uint8_t> emptyData{};
		emptyData.resize(buffer->ByteSize);
		memset(emptyData.data(), 0, buffer->ByteSize);
		GFX::Cmd::UploadToBuffer(context, buffer, 0, emptyData.data(), 0, buffer->ByteSize);
	}

	void Draw(GraphicsContext& context, uint32_t vertexCount, uint32_t vertexOffset)
	{
		PROFILE_CMD();
		context.CmdList->DrawInstanced(vertexCount, 1, vertexOffset, 0);
	}
	
	void DrawIndexed(GraphicsContext& context, uint32_t indexCount, uint32_t indexOffset, uint32_t vertexOffset)
	{
		PROFILE_CMD();
		context.CmdList->DrawIndexedInstanced(indexCount, 1, indexOffset, vertexOffset, 0);
	}

	void DrawInstanced(GraphicsContext& context, uint32_t vertexCount, uint32_t instanceCount, uint32_t vertexOffset, uint32_t firstInstance)
	{
		PROFILE_CMD();
		context.CmdList->DrawInstanced(vertexCount, instanceCount, vertexOffset, firstInstance);
	}
	
	void DrawIndexedInstanced(GraphicsContext& context, uint32_t indexCount, uint32_t instanceCount, uint32_t indexOffset, uint32_t vertexOffset, uint32_t firstInstance)
	{
		PROFILE_CMD();
		context.CmdList->DrawIndexedInstanced(indexCount, instanceCount, indexOffset, vertexOffset, firstInstance);
	}
	
	void Dispatch(GraphicsContext& context, uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ)
	{
		PROFILE_CMD();
		context.CmdList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
	}

	void ExecuteIndirect(GraphicsContext& context, ID3D12CommandSignature* commandSignature, uint32_t maxCommands, Buffer* argumentBuffer, uint32_t argumentOffset, Buffer* countBuffer, uint32_t countBufferOffset)
	{
		PROFILE_CMD();
		context.CmdList->ExecuteIndirect(commandSignature, maxCommands, argumentBuffer->Handle.Get(), argumentOffset, countBuffer ? countBuffer->Handle.Get() : nullptr, countBufferOffset);
	}

	void DrawFC(GraphicsContext& context, GraphicsState& state)
	{
		PROFILE_CMD();

		state.VertexBuffers[0] = GFX::RenderResources.QuadBuffer.get();
		context.ApplyState(state);
		context.CmdList->DrawInstanced(6, 1, 0, 0);
	}

	void GenerateMips(GraphicsContext& context, Texture* texture)
	{
		PROFILE_CMD();

		ASSERT(texture->DepthOrArraySize == 1, "GenerateMips not supported for texture arrays!");
		
		// Get staging texture
		StagingResourcesContext::StagingTextureRequest texRequest = {};
		texRequest.Width = texture->Width;
		texRequest.Height = texture->Height;
		texRequest.NumMips = texture->NumMips;
		texRequest.Format = texture->Format;
		texRequest.CreationFlags = RCF::RTV | RCF::GenerateMips;
		StagingResourcesContext::StagingTexture* stagingTexture = context.StagingResources.GetTransientTexture(texRequest);

		// Copy data to staging texture
		GFX::Cmd::CopyToTexture(context, texture, stagingTexture->TextureResource, 0);

		// Init subresources
		std::vector<TextureSubresourceView*> mipSubresources;
		mipSubresources.resize(texture->NumMips);
		for (uint32_t mip = 0; mip < texture->NumMips; mip++)
		{
			mipSubresources[mip] = GFX::GetTextureSubresource(stagingTexture->TextureResource, mip, mip, 0, 0);
		}

		// Init state
		GraphicsState state{};
		state.Shader = GFX::RenderResources.CopyShader.get();
		state.Table.SMPs[0] = Sampler{ D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
		
		// Generate mips
		for (uint32_t mip = 1; mip < texture->NumMips; mip++)
		{
			state.Table.SRVs[0] = mipSubresources[mip - 1];
			state.RenderTargets[0] = mipSubresources[mip];
			GFX::Cmd::DrawFC(context, state);
		}

		// Free subresources
		for (uint32_t mip = 0; mip < texture->NumMips; mip++)
		{
			GFX::Cmd::TransitionResource(context, mipSubresources[mip], mipSubresources[mip]->Parent->CurrState);
			delete mipSubresources[mip];
		}

		// Copy to target texture
		for (uint32_t mip = 0; mip < texture->NumMips; mip++) 
			GFX::Cmd::CopyToTexture(context, stagingTexture->TextureResource, texture, mip);
	}

	void ResolveTexture(GraphicsContext& context, Texture* inputTexture, Texture* outputTexture)
	{
		PROFILE_CMD();

		TransitionResource(context, inputTexture, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		TransitionResource(context, outputTexture, D3D12_RESOURCE_STATE_RESOLVE_DEST);
		context.CmdList->ResolveSubresource(outputTexture->Handle.Get(), 0, inputTexture->Handle.Get(), 0, outputTexture->Format);
	}

	void AddReadbackRequest(GraphicsContext& context, ReadbackBuffer* readbackBuffer)
	{
		PROFILE_CMD();

		GFX::Cmd::CopyToBuffer(context, readbackBuffer->GetWriteBuffer(), 0, readbackBuffer->Private_GetReadBufferForCopy(), 0, readbackBuffer->GetStride());
		context.PendingReadbacks.push_back(readbackBuffer);
	}

	template<typename T>
	BindlessTable CreateBindlessTable(GraphicsContext& context, std::vector<T*> resources, uint32_t registerSpace)
	{
		PROFILE_CMD();

		ASSERT(registerSpace > 0u, "[CreateBindlessTable] Bindless resources must exist on space that is not 0");
		ASSERT(!resources.empty(), "[CreateBindlessTable] Cannot create bindless table with no resources!");

		BindlessTable table;
		table.DescriptorTable = Device::Get()->GetMemory().SRVHeapGPU->Allocate(resources.size());
		table.RegisterSpace = registerSpace;
		table.DescriptorCount = (uint32_t) resources.size();

		// Fill descriptor table
		for (uint32_t i = 0; i < resources.size(); i++)
		{
			const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor = resources[i]->SRV.GetCPUHandle();
			const D3D12_CPU_DESCRIPTOR_HANDLE dstDescriptor = table.DescriptorTable.GetCPUHandle(i);
			Device::Get()->GetHandle()->CopyDescriptorsSimple(1, dstDescriptor, srcDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		
		return table;
	}

	BindlessTable CreateBindlessTable(GraphicsContext& context, std::vector<Texture*> textures, uint32_t registerSpace) { return CreateBindlessTable<Texture>(context, textures, registerSpace); }
	BindlessTable CreateBindlessTable(GraphicsContext& context, std::vector<Buffer*> buffers, uint32_t registerSpace) { return CreateBindlessTable<Buffer>(context, buffers, registerSpace); }

	void ReleaseBindlessTable(GraphicsContext& context, const BindlessTable& table)
	{
		if (table.DescriptorTable.IsValid())
			GFX::Cmd::Delete(context, table.DescriptorTable);
	}

}

// From Common.h
namespace EnginePrivate
{
	ScopedSectionResolver::ScopedSectionResolver(const GraphicsContext& context, const std::string& sectonName):
		m_Context(context)
	{
		GFX::Cmd::MarkerBegin(m_Context, sectonName);
	}

	ScopedSectionResolver::~ScopedSectionResolver()
	{
		GFX::Cmd::MarkerEnd(m_Context);
	}
}