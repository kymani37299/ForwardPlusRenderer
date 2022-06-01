#pragma once

#include "Render/ResourceID.h"

class RenderGroup;
struct ID3D11DeviceContext;

struct VertexPipeline
{
	void Init(ID3D11DeviceContext* context);
	void Draw(ID3D11DeviceContext* context, RenderGroup& renderGroup, bool skipCulling = false);

	uint32_t InstanceCount = 0;
	uint32_t IndexCount = 0;
	BufferID MeshletIndexBuffer;
	BufferID DrawableInstanceBuffer;
	BufferID MeshletInstanceBuffer;
};

extern VertexPipeline VertPipeline;