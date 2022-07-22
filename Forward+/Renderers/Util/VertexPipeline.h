#pragma once

#include <Engine/Common.h>

struct RenderGroup;
struct GraphicsContext;
struct GraphicsState;
struct Buffer;

struct VertexPipeline
{
	VertexPipeline();
	~VertexPipeline();

	void Init(GraphicsContext& context);
	void Draw(GraphicsContext& context, GraphicsState& state, RenderGroup& renderGroup, bool skipCulling = false);

	uint32_t InstanceCount = 0;
	uint32_t IndexCount = 0;
	ScopedRef<Buffer> MeshletIndexBuffer;
	ScopedRef<Buffer> DrawableInstanceBuffer;
	ScopedRef<Buffer> MeshletInstanceBuffer;
};

extern VertexPipeline* VertPipeline;