#pragma once

#include "Render/RenderAPI.h"
#include "Render/Resource.h"

namespace GFX
{
	RenderTargetID CreateRenderTarget(uint32_t width, uint32_t height, bool useDepthStencil = false, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
}