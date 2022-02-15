#pragma once

#include "Render/ResourceID.h"

struct ID3D11Buffer;
struct ID3D11ShaderResourceView;

namespace GFX
{
	// Creation
	BufferID CreateBuffer(uint64_t byteSize, uint64_t elementStride, uint64_t creationFlags, const void* initData = nullptr);

	inline BufferID CreateIndexBuffer(uint64_t byteSize, uint32_t elementStride, const void* initData)
	{
		return CreateBuffer(byteSize, elementStride, RCF_Bind_IB, initData);
	}

	template<typename VertexType> 
	BufferID CreateVertexBuffer(uint32_t numElements, const VertexType* initData)
	{
		constexpr uint32_t vertStride = sizeof(VertexType);
		return CreateBuffer(vertStride * numElements, vertStride, RCF_Bind_VB, initData);
	}

	template<typename ConstantType>
	BufferID CreateConstantBuffer()
	{
		constexpr uint32_t stride = (sizeof(ConstantType) + 255) & ~255;
		return CreateBuffer(stride, stride, RCF_Bind_CB | RCF_CPU_Write);
	}

	// Info
	ID3D11Buffer* DX_GetBuffer(BufferID bufferID);
	ID3D11ShaderResourceView* DX_GetBufferSRV(BufferID bufferID);
	uint64_t GetNumBufferElements(BufferID bufferID);

}