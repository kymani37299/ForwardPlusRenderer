#pragma once

#include <Engine/Common.h>

struct RenderGroup;
struct GraphicsContext;
struct GraphicsState;
struct Buffer;
struct Shader;

class VertexPipeline
{
public:
	VertexPipeline();
	~VertexPipeline();

	void Init(GraphicsContext& context);
	void Draw(GraphicsContext& context, GraphicsState& state, RenderGroup& renderGroup, bool skipCulling = false);

private:
	void Draw_CPU(GraphicsContext& context, GraphicsState& state, RenderGroup& renderGroup, bool skipCulling);
	void Draw_GPU(GraphicsContext& context, GraphicsState& state, RenderGroup& renderGroup, bool skipCulling);

private:
	ScopedRef<Buffer> m_IndirectArgumentsBuffer;
	ScopedRef<Buffer> m_IndirectArgumentsCountBuffer;
	ScopedRef<Shader> m_PrepareArgsShader;
};

extern VertexPipeline* VertPipeline;