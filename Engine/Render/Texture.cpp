#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "Render/Device.h"
#include "Render/Resource.h"
#include "Render/Commands.h"

namespace GFX
{
	// Texture Loading
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
	}

	namespace
	{
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
			default: NOT_IMPLEMENTED;
			}
			return 0;
		}
	}

	TextureID LoadTextureHDR(const std::string& path, uint64_t creationFlags)
	{
		static constexpr DXGI_FORMAT TEXTURE_FORMAT = DXGI_FORMAT_R32G32B32A32_FLOAT;
		int width, height, bpp;
		void* texData = LoadTextureF(path, width, height, bpp);
		TextureID id = CreateTexture(width, height, creationFlags, 1, TEXTURE_FORMAT, texData);
		FreeTexture(texData);
		return id;
	}

	TextureID LoadTexture(ID3D11DeviceContext* context, const std::string& path, uint64_t creationFlags, uint32_t numMips)
	{
		static constexpr DXGI_FORMAT TEXTURE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
		int width, height, bpp;
		void* texData = LoadTexture(path, width, height, bpp);
		TextureID id;
		if (numMips == 1)
		{
			id = CreateTexture(width, height, creationFlags, numMips, TEXTURE_FORMAT, texData);
		}
		else
		{
			TextureID stagingTexture = CreateTexture(width, height, RCF_Bind_SRV | RCF_Bind_RTV | RCF_GenerateMips, numMips, TEXTURE_FORMAT);
			GFX::Cmd::UploadToTexture(context, texData, stagingTexture);
			context->GenerateMips(GFX::Storage::GetTexture(stagingTexture).SRV.Get());

			id = CreateTexture(width, height, creationFlags | RCF_CopyDest, numMips, TEXTURE_FORMAT);
			for (uint32_t mip = 0; mip < GetNumMips(stagingTexture); mip++) GFX::Cmd::CopyToTexture(context, stagingTexture, id, mip);
			GFX::Storage::Free(stagingTexture);
		}
		FreeTexture(texData);
		return id;
	}

	TextureID LoadCubemap(const std::string& path, uint64_t creationFlags)
	{
		// Load tex
		static constexpr DXGI_FORMAT TEXTURE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
		int width, height, bpp;
		void* texData = LoadTexture(path, width, height, bpp);

		// Prepare init data
		std::vector<const void*> initData;
		initData.resize(6);
		uint32_t byteSizePerImg = width * height * bpp / 6;
		uint8_t* bytePtr = (uint8_t*) texData;
		for (size_t i = 0; i < 6; i++) initData[i] = (const void*) (bytePtr + i * byteSizePerImg);

		// Create tex
		TextureID id = CreateTextureArray(width, height / 6, 6, creationFlags, 1, TEXTURE_FORMAT, initData);
		FreeTexture(texData);
		return id;
	}

	D3D11_TEXTURE2D_DESC CreateTextureDesc(const Texture& texture, uint64_t creationFlags)
	{
		D3D11_TEXTURE2D_DESC textureDesc = {};
		textureDesc.Width = texture.Width;
		textureDesc.Height = texture.Height;
		textureDesc.MipLevels = texture.NumMips;
		textureDesc.ArraySize = texture.ArraySize;
		textureDesc.Format = texture.Format;
		textureDesc.SampleDesc.Count = GetSampleCount(creationFlags);
		textureDesc.Usage = GetUsageFlags(creationFlags);
		textureDesc.BindFlags = GetBindFlags(creationFlags);
		textureDesc.MiscFlags = GetMiscFlags(creationFlags);
		textureDesc.CPUAccessFlags = GetCPUAccessFlags(creationFlags);
		return textureDesc;
	}

	TextureID CreateTexture(uint32_t width, uint32_t height, uint64_t creationFlags, uint32_t numMips, DXGI_FORMAT format, const void* initData)
	{
		if (creationFlags & RCF_Bind_DSV) format = DXGI_FORMAT_R24G8_TYPELESS;

		TextureID id;
		Texture& texture = Storage::CreateTexture(id);

		texture.Width = width;
		texture.Height = height;
		texture.ArraySize = 1;
		texture.NumMips = numMips == MAX_MIPS ? (uint32_t) std::log2(MAX(width, height)) + 1 : numMips;
		texture.Format = format;
		texture.RowPitch = texture.Width * ToBPP(texture.Format);
		texture.SlicePitch = texture.RowPitch * texture.Height;

		D3D11_SUBRESOURCE_DATA* initializationData = nullptr;
		if (initData)
		{
			ASSERT(numMips == 1, "[CreateTexture] Initializing data on texture with mips is not supported!");
			initializationData = new D3D11_SUBRESOURCE_DATA();
			initializationData->pSysMem = initData;
			initializationData->SysMemPitch = texture.RowPitch;
			initializationData->SysMemSlicePitch = texture.SlicePitch;
		}

		D3D11_TEXTURE2D_DESC textureDesc = CreateTextureDesc(texture, creationFlags);
		API_CALL(Device::Get()->GetHandle()->CreateTexture2D(&textureDesc, initializationData, texture.Handle.GetAddressOf()));
		SAFE_DELETE(initializationData);

		const bool useMultisampling = GetSampleCount(creationFlags) != 1;

		if (creationFlags & RCF_Bind_SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = texture.Format;
			srvDesc.ViewDimension = useMultisampling ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = texture.NumMips;
			srvDesc.Texture2D.MostDetailedMip = 0;

			// Hack
			if (srvDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
			{
				srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			}

			API_CALL(Device::Get()->GetHandle()->CreateShaderResourceView(texture.Handle.Get(), &srvDesc, texture.SRV.GetAddressOf()));
		}
		
		if (creationFlags & RCF_Bind_UAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = texture.Format;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = 0;

			// Hack
			if (uavDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
			{
				uavDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			}

			API_CALL(Device::Get()->GetHandle()->CreateUnorderedAccessView(texture.Handle.Get(), &uavDesc, texture.UAV.GetAddressOf()));
		}

		if (creationFlags & RCF_Bind_RTV)
		{
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = format;
			rtvDesc.ViewDimension = useMultisampling ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;;
			rtvDesc.Texture2D.MipSlice = 0;

			API_CALL(Device::Get()->GetHandle()->CreateRenderTargetView(texture.Handle.Get(), &rtvDesc, texture.RTV.GetAddressOf()));
		}

		if (creationFlags & RCF_Bind_DSV)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
			dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsvDesc.ViewDimension = useMultisampling ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;
			API_CALL(Device::Get()->GetHandle()->CreateDepthStencilView(texture.Handle.Get(), &dsvDesc, texture.DSV.GetAddressOf()));
		}

		return id;
	}

	TextureID CreateTextureArray(uint32_t width, uint32_t height, uint32_t arraySize, uint64_t creationFlags, uint32_t numMips, DXGI_FORMAT format, std::vector<const void*> initData)
	{
		TextureID id;
		Texture& texture = Storage::CreateTexture(id);
		texture.Width = width;
		texture.Height = height;
		texture.ArraySize = arraySize;
		texture.NumMips = numMips == MAX_MIPS ? (uint32_t)std::log2(MAX(width, height)) + 1 : numMips;
		texture.Format = format;
		texture.RowPitch = texture.Width * ToBPP(texture.Format);
		texture.SlicePitch = texture.RowPitch * texture.Height;

		D3D11_SUBRESOURCE_DATA* initializationData = nullptr;
		if (!initData.empty())
		{
			ASSERT(numMips == 1, "[CreateTextureArray] Initializing data on texture with mips is not supported!");
			ASSERT(initData.size() == arraySize, "[CreateTextureArray] initData size must match arraySize!");

			initializationData = new D3D11_SUBRESOURCE_DATA[initData.size()]();
			for (size_t i = 0; i < initData.size(); i++)
			{
				initializationData = new D3D11_SUBRESOURCE_DATA();
				initializationData->pSysMem = initData[i];
				initializationData->SysMemPitch = texture.RowPitch;
				initializationData->SysMemSlicePitch = texture.SlicePitch;
			}
		}

		D3D11_TEXTURE2D_DESC textureDesc = CreateTextureDesc(texture, creationFlags);
		API_CALL(Device::Get()->GetHandle()->CreateTexture2D(&textureDesc, initializationData, texture.Handle.GetAddressOf()));
		if (initializationData != nullptr) delete[] initializationData;

		const bool useMultisampling = GetSampleCount(creationFlags) != 1;

		if (creationFlags & RCF_Bind_SRV)
		{
			if (creationFlags & RCF_Cubemap)
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
				srvDesc.Format = texture.Format;
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels = texture.NumMips;
				srvDesc.TextureCube.MostDetailedMip = 0;
				API_CALL(Device::Get()->GetHandle()->CreateShaderResourceView(texture.Handle.Get(), &srvDesc, texture.SRV.GetAddressOf()));
			}
			else
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
				srvDesc.Format = texture.Format;
				srvDesc.ViewDimension = useMultisampling ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
				srvDesc.Texture2DArray.ArraySize = texture.ArraySize;
				srvDesc.Texture2DArray.FirstArraySlice = 0;
				srvDesc.Texture2DArray.MipLevels = texture.NumMips;
				srvDesc.Texture2DArray.MostDetailedMip = 0;
				API_CALL(Device::Get()->GetHandle()->CreateShaderResourceView(texture.Handle.Get(), &srvDesc, texture.SRV.GetAddressOf()));
			}
		}

		if (creationFlags & RCF_Bind_UAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = texture.Format;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
			uavDesc.Texture2DArray.ArraySize = texture.ArraySize;
			uavDesc.Texture2DArray.FirstArraySlice = 0;
			uavDesc.Texture2DArray.MipSlice = 0;
			API_CALL(Device::Get()->GetHandle()->CreateUnorderedAccessView(texture.Handle.Get(), &uavDesc, texture.UAV.GetAddressOf()));
		}

		return id;
	}

	uint32_t GetNumMips(TextureID textureID)
	{
		const Texture& texture = GFX::Storage::GetTexture(textureID);
		return texture.NumMips;
	}
}