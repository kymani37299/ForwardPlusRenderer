#pragma once

#include <unordered_map>

#include <Engine/Common.h>

#include "Scene/SceneGraph.h"

struct GraphicsContext;
struct GraphicsState;
struct Texture;
struct Buffer;
struct Shader;

struct RenderGroupCullingData;
struct ViewFrustum;

struct GeometryCullingInput
{
	GeometryCullingInput(Camera& cam):
		Cam(cam)
	{ }

	Texture* Depth = nullptr; // If null, only frustum culling will be performed
	Camera& Cam;
};

// TODO: Fix stats
// Bonus: Add stats for shadows too
class Culling
{
public:
	Culling();
	~Culling();

	void Init(GraphicsContext& context);
	void UpdateResources(GraphicsContext& context);

	void CullGeometries(GraphicsContext& context, GeometryCullingInput& input);
	void CullLights(GraphicsContext& context, Texture* depth);

	Buffer* GetVisibleLightsBuffer() const { return m_VisibleLightsBuffer.get(); }

private:
	void CullRenderGroupCPU(GraphicsContext& context, RenderGroupType rgType, GeometryCullingInput& input);
	void CullRenderGroupGPU(GraphicsContext& context, RenderGroupType rgType, GeometryCullingInput& input);

	void UpdateStats(GraphicsContext& context, CameraCullingData& cullingData);

private:

	// Light culling
	uint32_t m_NumTilesX = 0;
	uint32_t m_NumTilesY = 0;

	ScopedRef<Shader> m_LightCullingShader;
	ScopedRef<Buffer> m_VisibleLightsBuffer;

	ScopedRef<Shader> m_GeometryCullingShader;
};