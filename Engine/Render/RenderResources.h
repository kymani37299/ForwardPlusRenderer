#pragma once

#include "Render/RenderAPI.h"

struct Shader;
struct Buffer;
struct GraphicsContext;

namespace GFX
{
	struct RenderingResources
	{
		ScopedRef<Shader> CopyShader;
		ScopedRef<Buffer> QuadBuffer;
	};

	void InitRenderingResources();
	void DestroyRenderingResources();

	extern RenderingResources RenderResources;
}