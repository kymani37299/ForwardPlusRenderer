#pragma once

#include "Render/ResourceID.h"

struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11DeviceContext;

namespace GFX
{
	// Creation
	BufferID CreateBuffer(uint32_t byteSize, uint32_t elementStride, uint64_t creationFlags, const void* initData = nullptr);
	void ResizeBuffer(ID3D11DeviceContext* context, BufferID bufferID, uint32_t byteSize);

	inline BufferID CreateIndexBuffer(uint32_t byteSize, uint32_t elementStride, const void* initData)
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
	ID3D11Buffer* DX_Buffer(BufferID bufferID);
	ID3D11ShaderResourceView* DX_SRV(BufferID bufferID);
	uint32_t GetNumElements(BufferID bufferID);

}