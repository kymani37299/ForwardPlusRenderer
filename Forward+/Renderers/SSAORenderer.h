#pragma once

#include <vector>
#include <Engine/Common.h>

struct GraphicsContext;
struct GraphicsState;
struct Texture;
struct Buffer;
struct Shader;

class SSAORenderer
{
public:
	SSAORenderer();
	~SSAORenderer();

	void Init(GraphicsContext& context);
	Texture* Draw(GraphicsContext& context, Texture* depth);

	void UpdateResources(GraphicsContext& context);

private:
	std::vector<Float4> m_Kernel;
	ScopedRef<Texture> m_NoiseTexture;
	ScopedRef<Shader> m_Shader;
	ScopedRef<Texture> m_SSAOSampleTexture;
	ScopedRef<Texture> m_SSAOTexture;
	ScopedRef<Texture> m_NoSSAOTexture;
};