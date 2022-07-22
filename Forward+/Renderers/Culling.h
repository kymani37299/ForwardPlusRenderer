#pragma once

#include <Engine/Common.h>

struct GraphicsContext;
struct GraphicsState;
struct Texture;
struct Buffer;
struct Shader;

class Culling
{
public:
	Culling();
	~Culling();

	void Init(GraphicsContext& context);
	void UpdateResources(GraphicsContext& context);

	void CullGeometries(GraphicsContext& context);
	void CullLights(GraphicsContext& context, Texture* depth);

	Buffer* GetVisibleLightsBuffer() const { return m_VisibleLightsBuffer.get(); }
private:

	// Light culling
	uint32_t m_NumTilesX = 0;
	uint32_t m_NumTilesY = 0;

	ScopedRef<Shader> m_LightCullingShader;
	ScopedRef<Buffer> m_VisibleLightsBuffer;
};