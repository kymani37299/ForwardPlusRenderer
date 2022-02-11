#include "Texture.h"

#include "Render/Device.h"
#include "Render/Resource.h"

namespace GFX
{
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

	RenderTargetID CreateRenderTarget(uint32_t width, uint32_t height, bool useDepthStencil, DXGI_FORMAT format)
	{
		RenderTargetID id;
		Texture& colorTexture = GFX::Storage::CreateTexture(id.ColorTexture);
		colorTexture.Format = format;
		colorTexture.Width = width;
		colorTexture.Height = height;

		{
			D3D11_TEXTURE2D_DESC textureDesc = {};
			textureDesc.Width = width;
			textureDesc.Height = height;
			textureDesc.MipLevels = 1;
			textureDesc.ArraySize = 1;
			textureDesc.Format = format;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.Usage = D3D11_USAGE_DEFAULT;
			textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			textureDesc.MiscFlags = 0;
			textureDesc.CPUAccessFlags = 0;
			API_CALL(Device::Get()->GetHandle()->CreateTexture2D(&textureDesc, nullptr, colorTexture.Handle.GetAddressOf()));

			D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
			renderTargetViewDesc.Format = format;
			renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			renderTargetViewDesc.Texture2D.MipSlice = 0;
			API_CALL(Device::Get()->GetHandle()->CreateRenderTargetView(colorTexture.Handle.Get(), &renderTargetViewDesc, colorTexture.RTV.GetAddressOf()));

			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			API_CALL(Device::Get()->GetHandle()->CreateShaderResourceView(colorTexture.Handle.Get(), &srvDesc, colorTexture.SRV.GetAddressOf()));
		}

		if (useDepthStencil)
		{
			Texture& depthTexture = GFX::Storage::CreateTexture(id.DepthTexture);
			depthTexture.Format = DXGI_FORMAT_R24G8_TYPELESS;
			depthTexture.Width = width;
			depthTexture.Height = height;

			D3D11_TEXTURE2D_DESC textureDesc = {};
			textureDesc.Width = width;
			textureDesc.Height = height;
			textureDesc.MipLevels = 1;
			textureDesc.ArraySize = 1;
			textureDesc.Format = depthTexture.Format;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.Usage = D3D11_USAGE_DEFAULT;
			textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			textureDesc.MiscFlags = 0;
			textureDesc.CPUAccessFlags = 0;
			API_CALL(Device::Get()->GetHandle()->CreateTexture2D(&textureDesc, nullptr, depthTexture.Handle.GetAddressOf()));

			D3D11_DEPTH_STENCIL_VIEW_DESC dsViewDesc = {};
			dsViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			dsViewDesc.Texture2D.MipSlice = 0;
			API_CALL(Device::Get()->GetHandle()->CreateDepthStencilView(depthTexture.Handle.Get(), &dsViewDesc, depthTexture.DSV.GetAddressOf()));
		}

		return id;
	}
}