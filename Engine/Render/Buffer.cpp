#include "Buffer.h"

#include "Render/Context.h"
#include "Render/Commands.h"
#include "Render/Device.h"
#include "Render/Memory.h"
#include "Utility/StringUtility.h"

namespace GFX
{
	static void CreateBufferResources(Buffer* buffer, ResourceInitData* initData)
	{
		Device* device = Device::Get();
		DeviceMemory& memory = device->GetMemory();

		D3D12_RESOURCE_DESC bufferDesc;
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Alignment = 0;
		bufferDesc.Width = buffer->ByteSize;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.SampleDesc.Quality = 0;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufferDesc.Flags = GetResourceCreationFlags(buffer->CreationFlags);

		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = GetHeapType(buffer->CreationFlags);
		API_CALL(device->GetAllocator()->CreateResource(&allocationDesc, &bufferDesc, buffer->CurrState, nullptr, &buffer->Alloc, IID_PPV_ARGS(buffer->Handle.GetAddressOf())));
		buffer->GPUAddress = buffer->Handle->GetGPUVirtualAddress();

		if (initData)
		{
			GFX::Cmd::UploadToBuffer(*initData->Context, buffer, 0, initData->Data, 0, buffer->ByteSize);
		}

		const bool rawBuffer = buffer->CreationFlags & RCF_Bind_RAW;

		// SRV
		{
			buffer->SRV = memory.SRVHeap->Alloc();

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;

			if (rawBuffer)
			{
				srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
				srvDesc.Buffer.NumElements = buffer->ByteSize / sizeof(uint32_t);
				srvDesc.Buffer.StructureByteStride = 0;
			}
			else
			{
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				srvDesc.Buffer.NumElements = buffer->ByteSize / buffer->Stride;
				srvDesc.Buffer.StructureByteStride = buffer->Stride;
			}

			device->GetHandle()->CreateShaderResourceView(buffer->Handle.Get(), &srvDesc, buffer->SRV);
		}

		if (buffer->CreationFlags & RCF_Bind_UAV)
		{
			buffer->UAV = memory.SRVHeap->Alloc();

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.CounterOffsetInBytes = 0;
			uavDesc.Buffer.FirstElement = 0;

			if (rawBuffer)
			{
				uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
				uavDesc.Buffer.NumElements = buffer->ByteSize / sizeof(uint32_t);
				uavDesc.Buffer.StructureByteStride = 0;
			}
			else
			{
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				uavDesc.Buffer.NumElements = buffer->ByteSize / buffer->Stride;
				uavDesc.Buffer.StructureByteStride = buffer->Stride;

			}

			device->GetHandle()->CreateUnorderedAccessView(buffer->Handle.Get(), nullptr, &uavDesc, buffer->UAV);
		}

		if (buffer->CreationFlags & RCF_Bind_CBV)
		{
			buffer->CBV = memory.SRVHeap->Alloc();

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
			cbvDesc.BufferLocation = buffer->GPUAddress;
			cbvDesc.SizeInBytes = buffer->ByteSize;
			device->GetHandle()->CreateConstantBufferView(&cbvDesc, buffer->CBV);
		}
	}

	D3D12_RESOURCE_STATES GetStartingBufferState(uint64_t creationFlags)
	{
		if (creationFlags & RCF_Readback) return D3D12_RESOURCE_STATE_COPY_DEST;
		if (creationFlags & RCF_CPU_Access) return D3D12_RESOURCE_STATE_GENERIC_READ;
		return D3D12_RESOURCE_STATE_COMMON;
	}

	Buffer* CreateBuffer(uint32_t byteSize, uint32_t elementStride, uint64_t creationFlags, ResourceInitData* initData)
	{
		Buffer* buffer = new Buffer{};
		buffer->Type = ResourceType::Buffer;
		buffer->CreationFlags = creationFlags;
		buffer->ByteSize = byteSize;
		buffer->Stride = elementStride;
		buffer->CurrState = GetStartingBufferState(creationFlags);
		CreateBufferResources(buffer, initData);
		return buffer;
	}

	void ResizeBuffer(GraphicsContext& context, Buffer* buffer, uint32_t byteSize)
	{
		Device* device = Device::Get();
		DeviceMemory& memory = device->GetMemory();

		Resource* oldResource = new Resource{};
		oldResource->Handle = buffer->Handle;
		oldResource->Alloc = buffer->Alloc;
		oldResource->CurrState = buffer->CurrState;
		oldResource->CBV = buffer->CBV;
		oldResource->SRV = buffer->SRV;
		oldResource->UAV = buffer->UAV;
		DeferredTrash::Put(oldResource);

		const uint32_t copySize = MIN(buffer->ByteSize, byteSize);
		
		buffer->Handle.Reset();
		buffer->Alloc.Reset();
		buffer->CurrState = buffer->CreationFlags & RCF_CPU_Access ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;
		buffer->ByteSize = byteSize;
		
		CreateBufferResources(buffer, nullptr);

		GFX::Cmd::TransitionResource(context, buffer, D3D12_RESOURCE_STATE_COPY_DEST);
		GFX::Cmd::TransitionResource(context, oldResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
		context.CmdList->CopyBufferRegion(buffer->Handle.Get(), 0, oldResource->Handle.Get(), 0, copySize);
	}

	// From Resource.h
	void SetDebugName(Resource* res, const std::string& name)
	{
#ifdef DEBUG
		res->DebugName = name;
		std::wstring wName = StringUtility::ToWideString(name);
		res->Handle->SetName(wName.c_str());
#endif
	}

}