#pragma once

#include "Render/ResourceID.h"

struct ID3D11Buffer;

namespace GFX
{
	// Creation
	BufferID CreateVertexBuffer(uint64_t byteSize, uint64_t elementStride, const void* initData);
	BufferID CreateIndexBuffer(uint64_t byteSize, uint32_t elementStride, const void* initData);
	BufferID CreateConstantBuffer(uint64_t bufferSize);

	template<typename VertexType> 
	BufferID CreateVertexBuffer(uint32_t numElements, VertexType* initData)
	{
		uint32_t vertStride = sizeof(VertexType);
		return CreateVertexBuffer(vertStride * numElements, vertStride, initData);
	}

	// Info
	ID3D11Buffer* DX_GetBuffer(BufferID bufferID);
	uint64_t GetNumBufferElements(BufferID bufferID);

}