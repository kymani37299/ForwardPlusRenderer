#pragma once

#include <unordered_map>
#include <vector>

#include "Render/ResourceID.h"
#include "Render/RenderAPI.h"

inline D3D11_USAGE GetUsageFlags(uint64_t creationFlags)
{
	if (creationFlags & RCF_CPU_Write) return D3D11_USAGE_DYNAMIC;
	if (creationFlags & RCF_CPU_Read) return D3D11_USAGE_DEFAULT;
	if (creationFlags & RCF_CPU_Write_Persistent) return D3D11_USAGE_DEFAULT;
	if (creationFlags & RCF_Bind_UAV) return D3D11_USAGE_DEFAULT;
	if (creationFlags & RCF_Bind_RTV) return D3D11_USAGE_DEFAULT;
	if (creationFlags & RCF_Bind_DSV) return D3D11_USAGE_DEFAULT;
	if (creationFlags & RCF_CopyDest) return D3D11_USAGE_DEFAULT;
	if (creationFlags & RCF_Staging) return D3D11_USAGE_STAGING;
	return D3D11_USAGE_IMMUTABLE;
}

inline uint32_t GetBindFlags(uint64_t creationFlags)
{
	uint32_t flags = 0;
	if (creationFlags & RCF_Bind_VB) flags  |= D3D11_BIND_VERTEX_BUFFER;
	if (creationFlags & RCF_Bind_IB) flags  |= D3D11_BIND_INDEX_BUFFER;
	if (creationFlags & RCF_Bind_CB) flags  |= D3D11_BIND_CONSTANT_BUFFER;
	if (creationFlags & RCF_Bind_SB) flags  |= D3D11_BIND_SHADER_RESOURCE;
	if (creationFlags & RCF_Bind_SRV) flags |= D3D11_BIND_SHADER_RESOURCE;
	if (creationFlags & RCF_Bind_UAV) flags |= D3D11_BIND_UNORDERED_ACCESS;
	if (creationFlags & RCF_Bind_RTV) flags |= D3D11_BIND_RENDER_TARGET;
	if (creationFlags & RCF_Bind_DSV) flags |= D3D11_BIND_DEPTH_STENCIL;
	return flags;
}

inline uint32_t GetCPUAccessFlags(uint64_t creationFlags)
{
	uint32_t flags = 0;
	if ((creationFlags & RCF_CPU_Write) || (creationFlags & RCF_CPU_Write_Persistent)) flags |= D3D11_CPU_ACCESS_WRITE;
	if (creationFlags & RCF_CPU_Read) flags |= D3D11_CPU_ACCESS_READ;
	return flags;
}

inline uint32_t GetMiscFlags(uint64_t creationFlags)
{
	uint32_t flags = 0;
	if (creationFlags & RCF_Bind_SB)		flags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	if (creationFlags & RCF_GenerateMips)	flags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	if (creationFlags & RCF_Cubemap)		flags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
	return flags;
}

inline uint32_t GetSampleCount(uint32_t creationFlags)
{
	if (creationFlags & RCF_MSAA_X2) return 2;
	if (creationFlags & RCF_MSAA_X4) return 4;
	if (creationFlags & RCF_MSAA_X8) return 8;
	if (creationFlags & RCF_MSAA_X16) return 16;
	return 1;
}

struct Buffer
{
	// DX
	ComPtr<ID3D11Buffer> Handle;
	ComPtr<ID3D11ShaderResourceView> SRV;
	ComPtr<ID3D11UnorderedAccessView> UAV;

	// Data
	uint32_t ByteSize;
	uint32_t ElementStride;

	uint64_t CreationFlags;
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
	uint32_t ArraySize;
	uint32_t NumMips;

	uint32_t RowPitch;
	uint32_t SlicePitch;
};

struct ShaderImplementation
{
	ComPtr<ID3D11VertexShader> VS;
	ComPtr<ID3D11PixelShader> PS;
	ComPtr<ID3D11ComputeShader> CS;
	ComPtr<ID3D11InputLayout> IL;
	ComPtr<ID3D11InputLayout> MIL;
};

struct Shader
{
	std::string Path;
	std::unordered_map<uint32_t, ShaderImplementation> Implementations;
};

namespace GFX
{
	namespace Storage
	{
		static constexpr uint32_t TEXTURE_STORAGE_SIZE = 1024;
		static constexpr uint32_t BUFFER_STORAGE_SIZE = 1024;
		static constexpr uint32_t SHADER_STORAGE_SIZE = 1024;

		const Buffer& GetBuffer(BufferID id);
		const Texture& GetTexture(TextureID id);
		const Shader& GetShader(ShaderID id);

		Buffer& CreateBuffer(BufferID& id);
		Texture& CreateTexture(TextureID& id);
		Shader& CreateShader(ShaderID& id);

		void Free(BufferID& id);
		void Free(TextureID& id);
		void Free(ShaderID& id);

		void ReloadAllShaders();
		void ClearStorage();
	}
}