#pragma once

#include <Engine/Common.h>

struct RenderGroup;
struct GraphicsContext;
struct GraphicsState;
struct Buffer;
struct Shader;

struct RenderGroup;
struct RenderGroupCullingData;

class VertexPipeline
{
public:
	VertexPipeline();
	~VertexPipeline();

	void Init(GraphicsContext& context);
	void Draw(GraphicsContext& context, GraphicsState& state, RenderGroup& renderGroup, RenderGroupCullingData& cullingData);

private:
	void Draw_CPU(GraphicsContext& context, GraphicsState& state, RenderGroup& renderGroup, RenderGroupCullingData& cullingData);
	void Draw_GPU(GraphicsContext& context, GraphicsState& state, RenderGroup& renderGroup, RenderGroupCullingData& cullingData);

private:
	ScopedRef<Buffer> m_IndirectArgumentsBuffer;
	ScopedRef<Buffer> m_IndirectArgumentsCountBuffer;
	ScopedRef<Shader> m_PrepareArgsShader;
};

extern VertexPipeline* VertPipeline;