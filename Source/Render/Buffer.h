#pragma once

#include "Render/ResourceID.h"

namespace GFX
{
	BufferID CreateVertexBuffer(uint64_t byteSize, uint64_t elementStride, const void* initData);
}