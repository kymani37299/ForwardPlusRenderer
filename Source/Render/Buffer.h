#pragma once

#include "Render/ResourceID.h"

namespace GFX
{
	BufferID CreateVertexBuffer(uint64_t byteSize, uint64_t elementStride, const void* initData);
	BufferID CreateIndexBuffer(uint64_t byteSize, uint32_t elementStride, const void* initData);

	template<typename VertexType> 
	BufferID CreateVertexBuffer(uint32_t numElements, VertexType* initData)
	{
		uint32_t vertStride = sizeof(VertexType);
		return CreateVertexBuffer(vertStride * numElements, vertStride, initData);
	}
}