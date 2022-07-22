#pragma once

#include "Render/Resource.h"

struct Buffer : public Resource 
{
	uint64_t ByteSize;
	uint64_t Stride;
	D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;
};

namespace GFX
{
	Buffer* CreateBuffer(uint64_t byteSize, uint64_t elementStride, uint64_t creationFlags, ResourceInitData* initData = nullptr);
	void ResizeBuffer(GraphicsContext& context, Buffer* buffer, uint64_t byteSize);

	inline void ExpandBuffer(GraphicsContext& context, Buffer* buffer, uint64_t byteSize)
	{
		if (byteSize > buffer->ByteSize)
		{
			ResizeBuffer(context, buffer, byteSize);
		}
	}

	inline Buffer* CreateIndexBuffer(uint32_t byteSize, uint32_t elementStride, ResourceInitData* initData)
	{
		return CreateBuffer(byteSize, elementStride, RCF_None, initData);
	}

	template<typename VertexType>
	Buffer* CreateVertexBuffer(uint32_t numElements, ResourceInitData* initData)
	{
		constexpr uint32_t vertStride = sizeof(VertexType);
		return CreateBuffer(vertStride * numElements, vertStride, RCF_None, initData);
	}

	template<typename ConstantType>
	Buffer* CreateConstantBuffer()
	{
		constexpr uint32_t stride = (sizeof(ConstantType) + 255) & ~255;
		return CreateBuffer(stride, stride, RCF_Bind_CBV | RCF_CPU_Access);
	}
}