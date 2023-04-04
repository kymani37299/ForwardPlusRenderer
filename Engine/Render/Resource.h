#pragma once

#include <unordered_map>

#include "Common.h"
#include "Render/RenderAPI.h"
#include "Render/DescriptorHeap.h"

struct GraphicsContext;

// RCF = Resource Creation Flags
DEFINE_ENUM_CLASS_FLAGS_EX(RCF, uint64_t);
enum class RCF : uint64_t
{
	None = 0,

	// Bindings
	UAV = 1,
	RTV = UAV << 1,
	DSV = RTV << 1,
	CBV = DSV << 1,
	RAW = CBV << 1,
	NoSRV = RAW << 1,

	// Misc
	CPU_Access = NoSRV << 1,
	Readback = CPU_Access << 1,
	GenerateMips = Readback << 1,
	Cubemap = GenerateMips << 1,
	Texture3D = Cubemap << 1,

	// MSAA
	MSAA_X2 = Texture3D << 1,
	MSAA_X4 = MSAA_X2 << 1,
	MSAA_X8 = MSAA_X4 << 1,
	MSAA_X16 = MSAA_X8 << 1,
};

inline D3D12_RESOURCE_FLAGS GetResourceCreationFlags(RCF creationFlags)
{
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	if (TestFlag(creationFlags, RCF::RTV)) flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (TestFlag(creationFlags, RCF::DSV)) flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (TestFlag(creationFlags, RCF::UAV)) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	return flags;
}

inline uint32_t GetSampleCount(RCF creationFlags)
{
	if (TestFlag(creationFlags, RCF::MSAA_X2)) return 2;
	if (TestFlag(creationFlags, RCF::MSAA_X4)) return 4;
	if (TestFlag(creationFlags, RCF::MSAA_X8)) return 8;
	if (TestFlag(creationFlags, RCF::MSAA_X16)) return 16;
	return 1;
}

inline D3D12_HEAP_TYPE GetHeapType(RCF creationFlags)
{
	if (TestFlag(creationFlags, RCF::Readback)) return D3D12_HEAP_TYPE_READBACK;
	if (TestFlag(creationFlags, RCF::CPU_Access)) return D3D12_HEAP_TYPE_UPLOAD;
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
	RCF CreationFlags = RCF::None;

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
