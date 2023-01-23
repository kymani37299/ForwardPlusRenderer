#pragma once

#include <Engine/Common.h>

struct Texture;
struct Shader;
struct GraphicsContext;
struct Camera;

class HZBGenerator
{
public:
	void Init(GraphicsContext& context);
	Texture* GetHZB(GraphicsContext& context, Texture* depth, const Camera& camera);

private:
	void RecreateTextures(GraphicsContext& context, uint32_t width, uint32_t height);

private:
	uint32_t m_HZBMips = 0;
	
	ScopedRef<Shader> m_GenerateHZBShader;

	ScopedRef<Texture> m_ReprojectedDepth;
	ScopedRef<Texture> m_HZB;
};