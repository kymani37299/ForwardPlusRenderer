#pragma once

#include "Common.h"

struct Texture;
struct GraphicsContext;

namespace TextureLoading
{
	Texture* LoadTextureHDR(const std::string& path, uint64_t creationFlags);
	Texture* LoadTexture(GraphicsContext& context, const std::string& path, uint64_t creationFlags, uint32_t numMips = 1);
	Texture* LoadCubemap(const std::string& path, uint64_t creationFlags);
}