#pragma once

#include <unordered_map>

#include "Common.h"
#include "Render/RenderAPI.h"
#include "Render/DescriptorHeap.h"

struct GraphicsContext;

enum ResourceCreationFlags : uint64_t
{
	RCF_None = 0,

	// Bindings
	RCF_Bind_UAV = 1,
	RCF_Bind_RTV = RCF_Bind_UAV << 1,
	RCF_Bind_DSV = RCF_Bind_RTV << 1,
	RCF_Bind_CBV = RCF_Bind_DSV << 1,
	RCF_Bind_RAW = RCF_Bind_CBV << 1,
	RCF_No_SRV = RCF_Bind_RAW << 1,

	// Misc
	RCF_CPU_Access = RCF_No_SRV << 1,
	RCF_Readback = RCF_CPU_Access << 1,
	RCF_GenerateMips = RCF_Readback << 1,
	RCF_Cubemap = RCF_GenerateMips << 1,
	RCF_Texture3D = RCF_Cubemap << 1,

	// MSAA
	RCF_MSAA_X2 = RCF_Texture3D << 1,
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

struct SubResource
{
	DescriptorAllocation CBV;
	DescriptorAllocation SRV;
	DescriptorAllocation UAV;
	DescriptorAllocation RTV;
	DescriptorAllocation DSV;
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
		if (Type == ResourceType::TextureSubresource || Type == ResourceType::BufferSubresource)
			return;

		if (CBV.IsValid()) CBV.Release();
		if (SRV.IsValid()) SRV.Release();
		if (UAV.IsValid()) UAV.Release();
		if (RTV.IsValid()) RTV.Release();
		if (DSV.IsValid()) DSV.Release();

		for (auto& it : Subresources)
		{
			SubResource& subres = it.second;
			if (subres.CBV.IsValid()) subres.CBV.Release();
			if (subres.SRV.IsValid()) subres.SRV.Release();
			if (subres.UAV.IsValid()) subres.UAV.Release();
			if (subres.RTV.IsValid()) subres.RTV.Release();
			if (subres.DSV.IsValid()) subres.DSV.Release();
		}
	}

	DescriptorAllocation CBV;
	DescriptorAllocation SRV;
	DescriptorAllocation UAV;
	DescriptorAllocation RTV;
	DescriptorAllocation DSV;

	std::unordered_map<uint32_t, SubResource> Subresources;

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
