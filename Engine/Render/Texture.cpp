#include "Texture.h"

#include "Render/Commands.h"
#include "Render/Device.h"
#include "Render/Resource.h"
#include "Render/DescriptorHeap.h"
#include "Utility/Hash.h"

namespace GFX
{
	static unsigned int ToBPP(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
		case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
		case DXGI_FORMAT_R11G11B10_FLOAT: return 4;
		case DXGI_FORMAT_R16G16_UNORM: return 4;
		case DXGI_FORMAT_R8G8B8A8_UNORM: return 4;
		case DXGI_FORMAT_R24G8_TYPELESS:  return 4;
		case DXGI_FORMAT_R32_FLOAT:  return 4;
		case DXGI_FORMAT_R16_FLOAT:  return 2;
		case DXGI_FORMAT_R16_UINT:  return 2;
		case DXGI_FORMAT_R16_UNORM:  return 2;
		default: NOT_IMPLEMENTED;
		}
		return 0;
	}

	// Depth format hack
	constexpr DXGI_FORMAT DepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	constexpr DXGI_FORMAT DepthViewFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

	static DescriptorAllocation CreateSRV(Texture* texture, uint32_t firstMip, uint32_t mipCount, uint32_t firstElement, uint32_t elementCount)
	{
		DescriptorAllocation SRV = Device::Get()->GetMemory().SRVHeap->Allocate();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = texture->Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		if (srvDesc.Format == DepthFormat)
		{
			srvDesc.Format = DepthViewFormat;
		}

		if (texture->DepthOrArraySize == 6 && TestFlag(texture->CreationFlags, RCF::Cubemap))
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = mipCount;
			srvDesc.TextureCube.MostDetailedMip = firstMip;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		}
		else if (TestFlag(texture->CreationFlags, RCF::Texture3D))
		{
			ASSERT(!GetSampleCount(texture->CreationFlags) != 1, "MS Texutre3D SRV not implemented");

			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srvDesc.Texture3D.MipLevels = mipCount;
			srvDesc.Texture3D.MostDetailedMip = firstMip;
			srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
		}
		else if (texture->DepthOrArraySize > 1)
		{
			ASSERT(!GetSampleCount(texture->CreationFlags) != 1, "MS Texutre array SRV not implemented");

			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = firstMip;
			srvDesc.Texture2DArray.MipLevels = mipCount;
			srvDesc.Texture2DArray.FirstArraySlice = firstElement;
			srvDesc.Texture2DArray.ArraySize = elementCount;
			srvDesc.Texture2DArray.PlaneSlice = 0;
			srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		}
		else
		{
			srvDesc.ViewDimension = GetSampleCount(texture->CreationFlags) != 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.MostDetailedMip = firstMip;
			srvDesc.Texture2D.MipLevels = mipCount;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		Device::Get()->GetHandle()->CreateShaderResourceView(texture->Handle.Get(), &srvDesc, SRV.GetCPUHandle());

		return SRV;
	}

	static DescriptorAllocation CreateUAV(Texture* texture, uint32_t firstMip, uint32_t firstElementOrSlice, uint32_t numSlices)
	{
		DescriptorAllocation UAV = Device::Get()->GetMemory().SRVHeap->Allocate();

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = texture->Format;

		if (uavDesc.Format == DepthFormat)
		{
			uavDesc.Format = DepthViewFormat;
		}

		if (TestFlag(texture->CreationFlags, RCF::Texture3D))
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			uavDesc.Texture3D.FirstWSlice = firstElementOrSlice;
			uavDesc.Texture3D.MipSlice = firstMip;
			uavDesc.Texture3D.WSize = numSlices;
		}
		else if (texture->DepthOrArraySize > 1)
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uavDesc.Texture2DArray.ArraySize = texture->DepthOrArraySize;
			uavDesc.Texture2DArray.FirstArraySlice = firstElementOrSlice;
			uavDesc.Texture2DArray.MipSlice = firstMip;
			uavDesc.Texture2DArray.PlaneSlice = 0;
		}
		else
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = firstMip;
			uavDesc.Texture2D.PlaneSlice = 0;
		}

		Device::Get()->GetHandle()->CreateUnorderedAccessView(texture->Handle.Get(), nullptr, &uavDesc, UAV.GetCPUHandle());

		return UAV;
	}

	static DescriptorAllocation CreateRTV(Texture* texture, uint32_t firstMip, uint32_t mipCount, uint32_t firstElement, uint32_t elementCount)
	{
		ASSERT(!TestFlag(texture->CreationFlags, RCF::Texture3D), "RTV not implemented for Texture3D!");
		ASSERT(texture->DepthOrArraySize == 1, "RTV not implemented for TextureArray!");

		DescriptorAllocation RTV = Device::Get()->GetMemory().RTVHeap->Allocate();

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = texture->Format;
		rtvDesc.ViewDimension = GetSampleCount(texture->CreationFlags) != 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = firstMip;
		rtvDesc.Texture2D.PlaneSlice = 0;
		Device::Get()->GetHandle()->CreateRenderTargetView(texture->Handle.Get(), &rtvDesc, RTV.GetCPUHandle());

		return RTV;
	}

	static DescriptorAllocation CreateDSV(Texture* texture, uint32_t firstMip, uint32_t mipCount, uint32_t firstElement, uint32_t elementCount)
	{
		ASSERT(texture->DepthOrArraySize == 1, "DSV not implemented for TextureArray!");

		DescriptorAllocation DSV = Device::Get()->GetMemory().DSVHeap->Allocate();

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DepthFormat;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = GetSampleCount(texture->CreationFlags) != 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = firstMip;
		Device::Get()->GetHandle()->CreateDepthStencilView(texture->Handle.Get(), &dsvDesc, DSV.GetCPUHandle());

		return DSV;
	}

	static void CreateTextureResources(Texture* texture, ResourceInitData* initData)
	{
		if (TestFlag(texture->CreationFlags, RCF::DSV)) texture->Format = DepthFormat;

		Device* device = Device::Get();
		DeviceMemory& memory = device->GetMemory();

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = TestFlag(texture->CreationFlags, RCF::Texture3D) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = texture->Width;
		resourceDesc.Height = texture->Height;
		resourceDesc.DepthOrArraySize = texture->DepthOrArraySize;
		resourceDesc.MipLevels = texture->NumMips;
		resourceDesc.Format = texture->Format;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = GetResourceCreationFlags(texture->CreationFlags);
		resourceDesc.SampleDesc.Count = GetSampleCount(texture->CreationFlags);
		resourceDesc.SampleDesc.Quality = 0;

		ScopedRef<D3D12_CLEAR_VALUE> clearValue = nullptr;
		if (TestFlag(texture->CreationFlags, RCF::RTV))
		{
			clearValue = ScopedRef<D3D12_CLEAR_VALUE>(new D3D12_CLEAR_VALUE{});
			clearValue->Color[0] = 0.0f;
			clearValue->Color[1] = 0.0f;
			clearValue->Color[2] = 0.0f;
			clearValue->Color[3] = 0.0f;
			clearValue->Format = texture->Format;
		}
		else if(TestFlag(texture->CreationFlags, RCF::DSV))
		{
			clearValue = ScopedRef<D3D12_CLEAR_VALUE>(new D3D12_CLEAR_VALUE{});
			clearValue->DepthStencil = { 1.0f, 0 };
			clearValue->Format = texture->Format;
		}

		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = GetHeapType(texture->CreationFlags);
		API_CALL(device->GetAllocator()->CreateResource(&allocationDesc, &resourceDesc, texture->CurrState, clearValue.get(), &texture->Alloc, IID_PPV_ARGS(texture->Handle.GetAddressOf())));

		if (initData) GFX::Cmd::UploadToTexture(*initData->Context, initData->Data, texture, 0);

		if(!TestFlag(texture->CreationFlags, RCF::NoSRV)) texture->SRV = CreateSRV(texture, 0, -1, 0, texture->DepthOrArraySize);
		if (TestFlag(texture->CreationFlags, RCF::UAV)) texture->UAV = CreateUAV(texture, 0, 0, texture->DepthOrArraySize);
		if (TestFlag(texture->CreationFlags, RCF::RTV) && !TestFlag(texture->CreationFlags, RCF::Cubemap)) texture->RTV = CreateRTV(texture, 0, texture->NumMips, -1, texture->DepthOrArraySize);
		if (TestFlag(texture->CreationFlags, RCF::DSV) && !TestFlag(texture->CreationFlags, RCF::Cubemap)) texture->DSV = CreateDSV(texture, 0, texture->NumMips, -1, texture->DepthOrArraySize);
	}

	Texture* CreateTexture(uint32_t width, uint32_t height, RCF creationFlags, uint32_t numMips, DXGI_FORMAT format, ResourceInitData* initData)
	{
		Texture* tex = new Texture{};
		tex->Type = ResourceType::Texture;
		tex->CreationFlags = creationFlags;
		tex->Format = format;
		tex->Width = width;
		tex->Height = height;
		tex->NumMips = numMips;
		tex->DepthOrArraySize = 1;
		tex->RowPitch = tex->Width * ToBPP(format);
		tex->SlicePitch = tex->RowPitch * height;
		tex->CurrState = D3D12_RESOURCE_STATE_COMMON;
		CreateTextureResources(tex, initData);
		return tex;
	}

	Texture* CreateTextureArray(uint32_t width, uint32_t height, uint32_t numElements, RCF creationFlags, uint32_t numMips, DXGI_FORMAT format, std::vector<ResourceInitData*> initData)
	{
		Texture* tex = new Texture{};
		tex->Type = ResourceType::Texture;
		tex->CreationFlags = creationFlags;
		tex->Format = format;
		tex->Width = width;
		tex->Height = height;
		tex->NumMips = numMips;
		tex->DepthOrArraySize = numElements;
		tex->RowPitch = width * ToBPP(format);
		tex->SlicePitch = tex->RowPitch * height;
		tex->CurrState = D3D12_RESOURCE_STATE_COMMON;
		CreateTextureResources(tex, nullptr);

		for (uint32_t i = 0; i < initData.size(); i++)
		{
			GFX::Cmd::UploadToTexture(*initData[i]->Context, initData[i]->Data, tex, 0, i);
		}

		return tex;
	}

	static uint32_t D3D12CalcSubresource(uint32_t MipSlice, uint32_t ArraySlice, uint32_t PlaneSlice, uint32_t MipLevels, uint32_t ArraySize)
	{
		return MipSlice + ArraySlice * MipLevels + PlaneSlice * MipLevels * ArraySize;
	}

	static uint32_t GetSubresourceHash(uint32_t firstMip, uint32_t lastMip, uint32_t firstElement, uint32_t lastElement)
	{
		uint32_t h = Hash::Crc32(firstMip);
		h = Hash::Crc32(h, lastMip);
		h = Hash::Crc32(h, firstElement);
		h = Hash::Crc32(h, lastElement);
		return h;
	}

	TextureSubresourceView* GetTextureSubresource(Texture* resource, uint32_t firstMip, uint32_t lastMip, uint32_t firstElement, uint32_t lastElement)
	{
		// The reason why its not supported its because we cannot decide what to put in Width and Height if it is mip range
		ASSERT(lastMip - firstMip + 1 == 1, "CreateTextureSubresource: Not supported mip range of more than 1 mip for now!");

		TextureSubresourceView* subres = new TextureSubresourceView{};
		subres->Type = ResourceType::TextureSubresource;
		subres->Handle = resource->Handle;
		subres->Alloc = resource->Alloc;
		subres->CurrState = resource->CurrState;
		subres->CreationFlags = resource->CreationFlags;
		subres->Format = resource->Format;
		subres->Width = resource->Width >> firstMip;
		subres->Height = resource->Height >> firstMip;
		subres->NumMips = resource->NumMips;
		subres->DepthOrArraySize = resource->DepthOrArraySize;
		subres->RowPitch = resource->RowPitch;
		subres->SlicePitch = resource->SlicePitch;
		subres->Parent = resource;
		subres->FirstMip = firstMip;
		subres->LastMip = lastMip;
		subres->FirstElement = firstElement;
		subres->LastElement = lastElement;

		const uint32_t subresHash = GetSubresourceHash(firstMip, lastMip, firstElement, lastElement);
		if (!resource->Subresources.contains(subresHash))
		{
			const uint32_t mipCount = lastMip - firstMip + 1;
			const uint32_t elementCount = lastElement - firstElement - 1;

			SubResource subresImpl{};
			if (resource->SRV.IsValid()) subresImpl.SRV = CreateSRV(subres, firstMip, mipCount, firstElement, elementCount);
			if (resource->UAV.IsValid()) subresImpl.UAV = CreateUAV(subres, firstMip, firstElement, elementCount);
			if (resource->RTV.IsValid()) subresImpl.RTV = CreateRTV(subres, firstMip, mipCount, firstElement, elementCount);
			if (resource->DSV.IsValid()) subresImpl.DSV = CreateDSV(subres, firstMip, mipCount, firstElement, elementCount);
			resource->Subresources[subresHash] = subresImpl;
		}

		const SubResource& subresImpl = resource->Subresources[subresHash];
		subres->SRV = subresImpl.SRV;
		subres->UAV = subresImpl.UAV;
		subres->RTV = subresImpl.RTV;
		subres->DSV = subresImpl.DSV;

		return subres;
	}

	uint32_t GetSubresourceIndex(Texture* texture, uint32_t mipIndex, uint32_t sliceOrArrayIndex)
	{
		if (TestFlag(texture->CreationFlags, RCF::Texture3D))
		{
			return D3D12CalcSubresource(mipIndex, 0, sliceOrArrayIndex, texture->NumMips, texture->DepthOrArraySize);
		}
		else
		{
			return D3D12CalcSubresource(mipIndex, sliceOrArrayIndex, 0, texture->NumMips, texture->DepthOrArraySize);
		}
	}
}