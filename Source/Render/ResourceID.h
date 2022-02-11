#pragma once

#include "Common.h"

using BufferID = uint32_t;
using TextureID = uint32_t;
using ShaderID = uint32_t;

static constexpr TextureID TextureID_Invalid = static_cast<TextureID>(-1);
static constexpr BufferID BufferID_Invalid = static_cast<BufferID>(-1);
static constexpr ShaderID ShaderID_Invalid = static_cast<ShaderID>(-1);

struct RenderTargetID
{
	TextureID ColorTexture = TextureID_Invalid;
	TextureID DepthTexture = TextureID_Invalid;
};

