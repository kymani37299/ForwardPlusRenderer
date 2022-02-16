#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "Render/Device.h"
#include "Render/Resource.h"

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
			case DXGI_FORMAT_R8G8B8A8_UNORM: return 4;
			case DXGI_FORMAT_R24G8_TYPELESS:  return 4;
			default: NOT_IMPLEMENTED;
			}
			return 0;
		}
	}

	TextureID LoadTexture(const std::string& path, uint64_t creationFlags)
	{
		int width, height, bpp;
		void* texData = LoadTexture(path, width, height, bpp);
		TextureID id = CreateTexture(width, height, creationFlags, DXGI_FORMAT_R8G8B8A8_UNORM, texData);
		FreeTexture(texData);
		return id;
	}

	TextureID CreateTexture(uint32_t width, uint32_t height, uint64_t creationFlags,  DXGI_FORMAT format, const void* initData)
	{
		if (creationFlags & RCF_Bind_DSV) format = DXGI_FORMAT_R24G8_TYPELESS;

		TextureID id;
		Texture& texture = Storage::CreateTexture(id);

		texture.Width = width;
		texture.Height = height;
		texture.NumMips = 1;
		texture.Format = format;
		texture.RowPitch = texture.Width * ToBPP(texture.Format);
		texture.SlicePitch = texture.RowPitch * texture.Height;

		D3D11_SUBRESOURCE_DATA* initializationData = nullptr;
		if (initData)
		{
			initializationData = new D3D11_SUBRESOURCE_DATA();
			initializationData->pSysMem = initData;
			initializationData->SysMemPitch = texture.RowPitch;
			initializationData->SysMemSlicePitch = texture.SlicePitch;
		}

		D3D11_TEXTURE2D_DESC textureDesc = {};
		textureDesc.Width = texture.Width;
		textureDesc.Height = texture.Height;
		textureDesc.MipLevels = texture.NumMips;
		textureDesc.ArraySize = 1;
		textureDesc.Format = texture.Format;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Usage = GetUsageFlags(creationFlags);
		textureDesc.BindFlags = GetBindFlags(creationFlags);
		textureDesc.MiscFlags = GetMiscFlags(creationFlags);
		textureDesc.CPUAccessFlags = GetCPUAccessFlags(creationFlags);
		API_CALL(Device::Get()->GetHandle()->CreateTexture2D(&textureDesc, initializationData, texture.Handle.GetAddressOf()));

		if (creationFlags & RCF_Bind_SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = texture.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			API_CALL(Device::Get()->GetHandle()->CreateShaderResourceView(texture.Handle.Get(), &srvDesc, texture.SRV.GetAddressOf()));
		}
		
		if (creationFlags & RCF_Bind_UAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = texture.Format;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = 0;
			API_CALL(Device::Get()->GetHandle()->CreateUnorderedAccessView(texture.Handle.Get(), &uavDesc, texture.UAV.GetAddressOf()));
		}

		if (creationFlags & RCF_Bind_RTV)
		{
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = format;
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;
			API_CALL(Device::Get()->GetHandle()->CreateRenderTargetView(texture.Handle.Get(), &rtvDesc, texture.RTV.GetAddressOf()));
		}

		if (creationFlags & RCF_Bind_DSV)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
			dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;
			API_CALL(Device::Get()->GetHandle()->CreateDepthStencilView(texture.Handle.Get(), &dsvDesc, texture.DSV.GetAddressOf()));
		}

		return id;
	}

	RenderTargetID CreateRenderTarget(uint32_t width, uint32_t height, bool useDepthStencil, DXGI_FORMAT format)
	{
		RenderTargetID id;
		id.ColorTexture = CreateTexture(width, height, RCF_Bind_RTV | RCF_Bind_SRV);
		if (useDepthStencil) { id.DepthTexture = CreateTexture(width, height, RCF_Bind_DSV); }
		return id;
	}

	ID3D11ShaderResourceView* DX_GetTextureSRV(TextureID textureID)
	{
		const Texture& texture = GFX::Storage::GetTexture(textureID);
		return texture.SRV.Get();
	}

	ID3D11SamplerState** GetStaticSamplers()
	{
		return (ID3D11SamplerState**) Device::Get()->GetStaticSamplers().data();
	}
	
	
	uint16_t GetStaticSamplersNum()
	{
		return Device::Get()->GetStaticSamplers().size();
	}
}