#pragma once

#include "Common.h"
#include "Render/Device.h"
#include "Render/RenderAPI.h"
#include "Render/Memory.h"
#include "Render/Context.h"

enum ResourceCreationFlags : uint64_t
{
	RCF_None = 0,

	// Bindings
	RCF_Bind_UAV = 1,
	RCF_Bind_RTV = RCF_Bind_UAV << 1,
	RCF_Bind_DSV = RCF_Bind_RTV << 1,
	RCF_Bind_CBV = RCF_Bind_DSV << 1,
	RCF_Bind_RAW = RCF_Bind_CBV << 1,

	// Misc
	RCF_CPU_Access = RCF_Bind_RAW << 1,
	RCF_Readback = RCF_CPU_Access << 1,
	RCF_GenerateMips = RCF_Readback << 1,
	RCF_Cubemap = RCF_GenerateMips << 1,

	// MSAA
	RCF_MSAA_X2 = RCF_Cubemap << 1,
	RCF_MSAA_X4 = RCF_MSAA_X2 << 1,
	RCF_MSAA_X8 = RCF_MSAA_X4 << 1,
	RCF_MSAA_X16 = RCF_MSAA_X8 << 1,
};

inline D3D12_RESOURCE_FLAGS GetResourceCreationFlags(uint64_t creationFlags)
{
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	if (creationFlags & RCF_Bind_RTV) flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (creationFlags & RCF_Bind_DSV) flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (creationFlags & RCF_Bind_UAV) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	return flags;
}

inline uint32_t GetSampleCount(uint64_t creationFlags)
{
	if (creationFlags & RCF_MSAA_X2) return 2;
	if (creationFlags & RCF_MSAA_X4) return 4;
	if (creationFlags & RCF_MSAA_X8) return 8;
	if (creationFlags & RCF_MSAA_X16) return 16;
	return 1;
}

inline D3D12_HEAP_TYPE GetHeapType(uint64_t creationFlags)
{
	if (creationFlags & RCF_Readback) return D3D12_HEAP_TYPE_READBACK;
	if (creationFlags & RCF_CPU_Access) return D3D12_HEAP_TYPE_UPLOAD;
	return D3D12_HEAP_TYPE_DEFAULT;
}

enum class ResourceType
{
	Invalid,
	Buffer,
	Texture,
	BufferSubresource,
	TextureSubresource,
};

struct Resource
{
	ResourceType Type = ResourceType::Invalid;
	uint64_t CreationFlags = RCF_None;

	ComPtr<ID3D12Resource> Handle = nullptr;
	ComPtr<D3D12MA::Allocation> Alloc = nullptr;
	D3D12_RESOURCE_STATES CurrState = D3D12_RESOURCE_STATE_COMMON;

	~Resource()
	{
		DeviceMemory& mem = Device::Get()->GetMemory();
		if(CBV.ptr != CPUAllocStrategy::INVALID) mem.SRVHeap->Release(CBV);
		if(SRV.ptr != CPUAllocStrategy::INVALID) mem.SRVHeap->Release(SRV);
		if(UAV.ptr != CPUAllocStrategy::INVALID) mem.SRVHeap->Release(UAV);
		if(RTV.ptr != CPUAllocStrategy::INVALID) mem.RTVHeap->Release(RTV);
		if(DSV.ptr != CPUAllocStrategy::INVALID) mem.DSVHeap->Release(DSV);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE CBV = { CPUAllocStrategy::INVALID };
	D3D12_CPU_DESCRIPTOR_HANDLE SRV = { CPUAllocStrategy::INVALID };
	D3D12_CPU_DESCRIPTOR_HANDLE UAV = { CPUAllocStrategy::INVALID };
	D3D12_CPU_DESCRIPTOR_HANDLE RTV = { CPUAllocStrategy::INVALID };
	D3D12_CPU_DESCRIPTOR_HANDLE DSV = { CPUAllocStrategy::INVALID };

#ifdef DEBUG
	std::string DebugName = "Unknown Resource";
#endif
};

struct ResourceInitData
{
	GraphicsContext* Context;
	const void* Data;
};

namespace GFX
{
	void SetDebugName(Resource* resource, const std::string& name);
}
