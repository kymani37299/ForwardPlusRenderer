#pragma once

#include "Common.h"

using BufferID = uint32_t;
using TextureID = uint32_t;

struct RenderTargetID
{
	TextureID ColorTexture = static_cast<TextureID>(-1);
	TextureID DepthTexture = static_cast<TextureID>(-1);
};

using ShaderID = uint32_t;