#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "Render/Commands.h"
#include "Render/Device.h"
#include "Render/Resource.h"
#include "Render/Memory.h"

namespace
{
	unsigned char INVALID_TEXTURE_COLOR[] = { 0xff, 0x00, 0x33, 0xff };

	void* LoadTexture(const std::string& path, int& width, int& height, int& bpp)
	{
		void* data = stbi_load(path.c_str(), &width, &height, &bpp, 4);

		if (!data)
		{
			std::cout << "Warning: Failed to load texture: " << path << std::endl;
			data = INVALID_TEXTURE_COLOR;
			width = 1;
			height = 1;
			bpp = 4;
		}

		return data;
	}
	void* LoadTextureF(const std::string& path, int& width, int& height, int& bpp)
	{
		void* data = stbi_loadf(path.c_str(), &width, &height, &bpp, 4);

		if (!data)
		{
			std::cout << "Warning: Failed to load texture: " << path << std::endl;
			data = INVALID_TEXTURE_COLOR;
			width = 1;
			height = 1;
			bpp = 4;
		}

		return data;
	}

	void FreeTexture(void* data)
	{
		if (data != INVALID_TEXTURE_COLOR)
			stbi_image_free(data);
	}

	unsigned int ToBPP(DXGI_FORMAT format)
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
}

namespace GFX
{
	// Depth format hack
	constexpr DXGI_FORMAT DepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	constexpr DXGI_FORMAT DepthViewFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

	static D3D12_CPU_DESCRIPTOR_HANDLE CreateSRV(Texture* texture, uint32_t firstMip, uint32_t mipCount, uint32_t firstElement, uint32_t elementCount)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE SRV = Device::Get()->GetMemory().SRVHeap->Alloc();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = texture->Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		if (srvDesc.Format == DepthFormat)
		{
			srvDesc.Format = DepthViewFormat;
		}

		if (texture->DepthOrArraySize == 6 && texture->CreationFlags & RCF_Cubemap)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = mipCount;
			srvDesc.TextureCube.MostDetailedMip = firstMip;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		}
		else if (texture->CreationFlags & RCF_Texture3D)
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

		Device::Get()->GetHandle()->CreateShaderResourceView(texture->Handle.Get(), &srvDesc, SRV);

		return SRV;
	}

	static D3D12_CPU_DESCRIPTOR_HANDLE CreateUAV(Texture* texture, uint32_t firstMip, uint32_t firstElementOrSlice, uint32_t numSlices)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE UAV = Device::Get()->GetMemory().SRVHeap->Alloc();

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = texture->Format;

		if (uavDesc.Format == DepthFormat)
		{
			uavDesc.Format = DepthViewFormat;
		}

		if (texture->CreationFlags & RCF_Texture3D)
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

		Device::Get()->GetHandle()->CreateUnorderedAccessView(texture->Handle.Get(), nullptr, &uavDesc, UAV);

		return UAV;
	}

	static D3D12_CPU_DESCRIPTOR_HANDLE CreateRTV(Texture* texture, uint32_t firstMip, uint32_t mipCount, uint32_t firstElement, uint32_t elementCount)
	{
		ASSERT(!(texture->CreationFlags & RCF_Texture3D), "RTV not implemented for Texture3D!");
		ASSERT(texture->DepthOrArraySize == 1, "RTV not implemented for TextureArray!");

		D3D12_CPU_DESCRIPTOR_HANDLE RTV = Device::Get()->GetMemory().RTVHeap->Alloc();

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = texture->Format;
		rtvDesc.ViewDimension = GetSampleCount(texture->CreationFlags) != 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = firstMip;
		rtvDesc.Texture2D.PlaneSlice = 0;
		Device::Get()->GetHandle()->CreateRenderTargetView(texture->Handle.Get(), &rtvDesc, RTV);

		return RTV;
	}

	static D3D12_CPU_DESCRIPTOR_HANDLE CreateDSV(Texture* texture, uint32_t firstMip, uint32_t mipCount, uint32_t firstElement, uint32_t elementCount)
	{
		ASSERT(texture->DepthOrArraySize == 1, "DSV not implemented for TextureArray!");

		D3D12_CPU_DESCRIPTOR_HANDLE DSV = Device::Get()->GetMemory().DSVHeap->Alloc();

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DepthFormat;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = GetSampleCount(texture->CreationFlags) != 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = firstMip;
		Device::Get()->GetHandle()->CreateDepthStencilView(texture->Handle.Get(), &dsvDesc, DSV);

		return DSV;
	}

	static void CreateTextureResources(Texture* texture, ResourceInitData* initData)
	{
		if (texture->CreationFlags & RCF_Bind_DSV) texture->Format = DepthFormat;

		Device* device = Device::Get();
		DeviceMemory& memory = device->GetMemory();

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = texture->CreationFlags & RCF_Texture3D ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
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
		if (texture->CreationFlags & RCF_Bind_RTV)
		{
			clearValue = ScopedRef<D3D12_CLEAR_VALUE>(new D3D12_CLEAR_VALUE{});
			clearValue->Color[0] = 0.0f;
			clearValue->Color[1] = 0.0f;
			clearValue->Color[2] = 0.0f;
			clearValue->Color[3] = 0.0f;
			clearValue->Format = texture->Format;
		}
		else if(texture->CreationFlags & RCF_Bind_DSV)
		{
			clearValue = ScopedRef<D3D12_CLEAR_VALUE>(new D3D12_CLEAR_VALUE{});
			clearValue->DepthStencil = { 1.0f, 0 };
			clearValue->Format = texture->Format;
		}

		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = GetHeapType(texture->CreationFlags);
		API_CALL(device->GetAllocator()->CreateResource(&allocationDesc, &resourceDesc, texture->CurrState, clearValue.get(), &texture->Alloc, IID_PPV_ARGS(texture->Handle.GetAddressOf())));

		if (initData) GFX::Cmd::UploadToTexture(*initData->Context, initData->Data, texture, 0);

		texture->SRV = CreateSRV(texture, 0, -1, 0, texture->DepthOrArraySize);
		if (texture->CreationFlags & RCF_Bind_UAV) texture->UAV = CreateUAV(texture, 0, 0, texture->DepthOrArraySize);
		if (texture->CreationFlags & RCF_Bind_RTV && !(texture->CreationFlags & RCF_Cubemap)) texture->RTV = CreateRTV(texture, 0, texture->NumMips, -1, texture->DepthOrArraySize);
		if (texture->CreationFlags & RCF_Bind_DSV && !(texture->CreationFlags & RCF_Cubemap)) texture->DSV = CreateDSV(texture, 0, texture->NumMips, -1, texture->DepthOrArraySize);
	}

	Texture* CreateTexture(uint32_t width, uint32_t height, uint64_t creationFlags, uint32_t numMips, DXGI_FORMAT format, ResourceInitData* initData)
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

	Texture* CreateTextureArray(uint32_t width, uint32_t height, uint32_t numElements, uint64_t creationFlags, uint32_t numMips, DXGI_FORMAT format, std::vector<ResourceInitData*> initData)
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

	Texture* LoadTextureHDR(const std::string& path, uint64_t creationFlags)
	{
		// TODO: Pass context
		static constexpr DXGI_FORMAT TEXTURE_FORMAT = DXGI_FORMAT_R32G32B32A32_FLOAT;
		int width, height, bpp;
		void* texData = LoadTextureF(path, width, height, bpp);
		ResourceInitData initData = { &Device::Get()->GetContext(), texData};
		Texture* texture = GFX::CreateTexture(width, height, creationFlags, 1, TEXTURE_FORMAT, &initData);
		FreeTexture(texData);
		return texture;
	}

	Texture* LoadTexture(GraphicsContext& context, const std::string& path, uint64_t creationFlags, uint32_t numMips)
	{
		static constexpr DXGI_FORMAT TEXTURE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

		int width, height, bpp;
		void* texData = ::LoadTexture(path, width, height, bpp);
		Texture* texture;
		if (numMips == 1)
		{
			ResourceInitData initData = { &context, texData };
			texture = GFX::CreateTexture(width, height, creationFlags, numMips, TEXTURE_FORMAT, &initData);
		}
		else
		{
			Texture* stagingTexture = GFX::CreateTexture(width, height, RCF_Bind_RTV | RCF_GenerateMips, numMips, TEXTURE_FORMAT);
			DeferredTrash::Put(stagingTexture);
			GFX::Cmd::UploadToTexture(context, texData, stagingTexture);
			GFX::Cmd::GenerateMips(context, stagingTexture);

			texture = GFX::CreateTexture(width, height, creationFlags, numMips, TEXTURE_FORMAT);
			for(uint32_t mip =0;mip < numMips;mip++) GFX::Cmd::CopyToTexture(context, stagingTexture, texture, mip);
		}
		FreeTexture(texData);
		return texture;
	}

	Texture* LoadCubemap(const std::string& path, uint64_t creationFlags)
	{
		// TODO: Pass context

		// Load tex
		static constexpr DXGI_FORMAT TEXTURE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
		int width, height, bpp;
		void* texData = ::LoadTexture(path, width, height, bpp);

		// Prepare init data
		std::vector<ResourceInitData*> initData;
		ResourceInitData datas[6];
		initData.resize(6);
		uint32_t byteSizePerImg = width * height * bpp / 6;
		uint8_t* bytePtr = (uint8_t*)texData;
		for (size_t i = 0; i < 6; i++)
		{
			datas[i] = { &Device::Get()->GetContext(), (const void*)(bytePtr + i * byteSizePerImg) };
			initData[i] = &datas[i];
		}

		// Create tex
		Texture* texture = GFX::CreateTextureArray(width, height / 6, 6, creationFlags, 1, TEXTURE_FORMAT, initData);
		FreeTexture(texData);
		return texture;
	}

	TextureSubresource* CreateTextureSubresource(Texture* resource, uint32_t firstMip, uint32_t mipCount, uint32_t firstElement, uint32_t elementCount)
	{
		// The reason why its not supported its because we cannot decide what to put in Width and Height if it is mip range
		ASSERT(mipCount == 1, "CreateTextureSubresource: Not supported mip range of more than 1 mip for now!");

		TextureSubresource* subres = new TextureSubresource{};
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
		subres->FirstMip = firstMip;
		subres->MipCount = mipCount;
		subres->FirstElement = firstElement;
		subres->ElementCount = elementCount;

		subres->SRV = CreateSRV(subres, firstMip, mipCount, firstElement, elementCount);
		if (subres->CreationFlags & RCF_Bind_UAV) subres->UAV = CreateUAV(subres, firstMip, firstElement, elementCount);
		if (subres->CreationFlags & RCF_Bind_RTV && !(subres->CreationFlags & RCF_Cubemap)) subres->RTV = CreateRTV(subres, firstMip, mipCount, firstElement, elementCount);
		if (subres->CreationFlags & RCF_Bind_DSV && !(subres->CreationFlags & RCF_Cubemap)) subres->DSV = CreateDSV(subres, firstMip, mipCount, firstElement, elementCount);

		return subres;
	}

	static uint32_t D3D12CalcSubresource(uint32_t MipSlice, uint32_t ArraySlice, uint32_t PlaneSlice, uint32_t MipLevels, uint32_t ArraySize)
	{
		return MipSlice + ArraySlice * MipLevels + PlaneSlice * MipLevels * ArraySize;
	}

	uint32_t GetSubresourceIndex(Texture* texture, uint32_t mipIndex, uint32_t sliceOrArrayIndex)
	{
		if (texture->CreationFlags & RCF_Texture3D)
		{
			return D3D12CalcSubresource(mipIndex, 0, sliceOrArrayIndex, texture->NumMips, texture->DepthOrArraySize);
		}
		else
		{
			return D3D12CalcSubresource(mipIndex, sliceOrArrayIndex, 0, texture->NumMips, texture->DepthOrArraySize);
		}
	}
}