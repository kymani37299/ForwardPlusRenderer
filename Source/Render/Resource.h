#pragma once

#include <vector>

#include "Render/ResourceID.h"
#include "Render/RenderAPI.h"

inline D3D11_USAGE GetUsageFlags(uint64_t creationFlags)
{
	if (creationFlags & RCF_CPU_Write) return D3D11_USAGE_DYNAMIC;
	if (creationFlags & RCF_CPU_Read) return D3D11_USAGE_DEFAULT;
	return D3D11_USAGE_IMMUTABLE;
}

inline uint32_t GetBindFlags(uint64_t creationFlags)
{
	uint32_t flags = 0;
	if (creationFlags & RCF_Bind_VB) flags  |= D3D11_BIND_VERTEX_BUFFER;
	if (creationFlags & RCF_Bind_IB) flags  |= D3D11_BIND_INDEX_BUFFER;
	if (creationFlags & RCF_Bind_CB) flags  |= D3D11_BIND_CONSTANT_BUFFER;
	if (creationFlags & RCF_Bind_SB) flags  |= D3D11_BIND_SHADER_RESOURCE;
	if (creationFlags & RCF_Bind_UAV) flags |= D3D11_BIND_SHADER_RESOURCE;
	return flags;
}

inline uint32_t GetCPUAccessFlags(uint64_t creationFlags)
{
	uint32_t flags = 0;
	if (creationFlags & RCF_CPU_Write) flags |= D3D11_CPU_ACCESS_WRITE;
	if (creationFlags & RCF_CPU_Read) flags |= D3D11_CPU_ACCESS_READ;
	return flags;
}

inline uint32_t GetMiscFlags(uint64_t creationFlags)
{
	uint32_t flags = 0;
	if (creationFlags & RCF_Bind_SB) flags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	return flags;
}

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

	std::string Path;
	std::vector<std::string> Defines;
	uint32_t CreationFlags;
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

		void ReloadAllShaders();
		void ClearStorage();
	}
}