#include "shared_definitions.h"
#include "scene.h"

struct VertexPipelineInput
{
    uint LocalOffset : LOCAL_OFFSET;
    uint MeshletInstance : I_MESHLET_INSTANCE;
    uint DrawableInstance : I_DRAWABLE_INSTANCE;
};

Texture2DArray Textures : register(t120);
StructuredBuffer<Vertex> Vertices : register(t121);
StructuredBuffer<uint> Indices : register(t122);
StructuredBuffer<Mesh> Meshes : register(t123);
StructuredBuffer<Entity> Entities : register(t124);
StructuredBuffer<Material> Materials : register(t125);
StructuredBuffer<Drawable> Drawables : register(t126);

Vertex GetWorldSpaceVertex(VertexPipelineInput input)
{
    const Drawable d = Drawables[input.DrawableInstance];
    const Mesh m = Meshes[d.MeshIndex];
    const uint index = m.IndexOffset + input.LocalOffset + input.MeshletInstance * MESHLET_INDEX_COUNT;
    Vertex vert = Vertices[Indices[index]]; // TODO: Delete index buffer indirection

    const float4x4 modelToWorld = Entities[d.EntityIndex].ModelToWorld;

    vert.Position = mul(float4(vert.Position, 1.0f), modelToWorld).xyz;
    vert.Normal = mul(vert.Normal, (float3x3)modelToWorld); // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.

    return vert;
}