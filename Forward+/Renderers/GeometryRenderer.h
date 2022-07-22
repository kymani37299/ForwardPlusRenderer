#pragma once

#include <Engine/Common.h>

struct GraphicsContext;
struct GraphicsState;
struct Texture;
struct Buffer;
struct Shader;

class GeometryRenderer
{
public:
	GeometryRenderer();
	~GeometryRenderer();

	void Init(GraphicsContext& context);
	void DepthPrepass(GraphicsContext& context, GraphicsState& state);
	void Draw(GraphicsContext& context, GraphicsState& state, Texture* shadowMask, Buffer* visibleLights, Texture* irradianceMap, Texture* ambientOcclusion);

private:
	ScopedRef<Shader> m_DepthPrepassShader;
	ScopedRef<Shader> m_GeometryShader;
};