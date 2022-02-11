#pragma once

#include "Render/ResourceID.h"
#include "Render/RenderAPI.h"

struct Buffer
{
	// DX
	ComPtr<ID3D11Buffer> Handle;
	ComPtr<ID3D11ShaderResourceView> SRV;
	ComPtr<ID3D11UnorderedAccessView> UAV;

	// Data
	uint64_t ByteSize;
	uint64_t ElementStride;
};

struct Texture
{
	// DX
	ComPtr<ID3D11Texture2D> Handle;
	ComPtr<ID3D11ShaderResourceView> SRV;
	ComPtr<ID3D11UnorderedAccessView> UAV;
	ComPtr<ID3D11RenderTargetView> RTV;
	ComPtr<ID3D11DepthStencilView> DSV;

	// Data
	DXGI_FORMAT Format;
	uint32_t Width;
	uint32_t Height;
	uint32_t NumMips;

	uint32_t RowPitch;
	uint32_t SlicePitch;
};

struct Shader
{
	ComPtr<ID3D11VertexShader> VS;
	ComPtr<ID3D11PixelShader> PS;
	ComPtr<ID3D11InputLayout> IL;
	ComPtr<ID3D11InputLayout> MIL;
};

namespace GFX
{
	namespace Storage
	{
		const Buffer& GetBuffer(BufferID id);
		const Texture& GetTexture(TextureID id);
		const Shader& GetShader(ShaderID id);

		Buffer& CreateBuffer(BufferID& id);
		Texture& CreateTexture(TextureID& id);
		Shader& CreateShader(ShaderID& id);

		void ClearStorage();
	}
}