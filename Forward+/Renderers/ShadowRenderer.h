#pragma once

#include <Engine/Common.h>

#include "Renderers/Util/HzbGenerator.h"

struct GraphicsContext;
struct GraphicsState;
struct Texture;
struct Buffer;
struct Shader;

class ShadowRenderer
{
public:
	ShadowRenderer();
	~ShadowRenderer();

	void Init(GraphicsContext& context);
	Texture* CalculateShadowMask(GraphicsContext& context, Texture* depth);
	void ReloadTextureResources(GraphicsContext& context);

	Texture* GetHZB(GraphicsContext& context);

private:
	ScopedRef<Shader> m_ShadowmapShader;
	ScopedRef<Shader> m_ShadowmaskShader;

	ScopedRef<Texture> m_Shadowmap;
	ScopedRef<Texture> m_Shadowmask;

	HZBGenerator m_HzbGenerator;
};