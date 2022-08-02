#pragma once

#include <Engine/Common.h>

struct RenderGroup;
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

	void CullGeometries(GraphicsContext& context, Texture* depth);
	void CullLights(GraphicsContext& context, Texture* depth);

	Buffer* GetVisibleLightsBuffer() const { return m_VisibleLightsBuffer.get(); }

private:
	void CullRenderGroupCPU(GraphicsContext& context, RenderGroup& rg);
	void CullRenderGroupGPU(GraphicsContext& context, RenderGroup& rg, Texture* depth);

	void UpdateStats(GraphicsContext& context);

private:

	// Light culling
	uint32_t m_NumTilesX = 0;
	uint32_t m_NumTilesY = 0;

	ScopedRef<Shader> m_LightCullingShader;
	ScopedRef<Buffer> m_VisibleLightsBuffer;

	ScopedRef<Shader> m_GeometryCullingShader;
	ScopedRef<Buffer> m_CullingStatsBuffer;
	ScopedRef<Buffer> m_CullingStatsReadback;
};