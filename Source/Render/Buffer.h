#pragma once

#include "Render/Resource.h"

namespace GFX
{
	BufferID CreateVertexBuffer(uint64_t byteSize, uint64_t elementStride, const void* initData);
}