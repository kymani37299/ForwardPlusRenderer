#ifndef PIPELINE_H
#define PIPELINE_H

#include "shared_definitions.h"
#include "scene.h"

struct VertexPipelineInput
{
	float3 Position : PIPELINE_POSITION;
	float2 Texcoord : PIPELINE_TEXCOORD;
	float3 Normal : PIPELINE_NORMAL;
	float4 Tangent : PIPELINE_TANGENT;
};

struct VertexPipelinePushConstants
{
    uint DrawableID;
};

StructuredBuffer<Material> Materials : register(t125);
StructuredBuffer<Drawable> Drawables : register(t126);
Texture2D Textures[] : register(t0, space1);

ConstantBuffer<VertexPipelinePushConstants> PushConstants : register(b128);

Drawable GetDrawable()
{
    return Drawables[PushConstants.DrawableID];
}

Vertex GetWorldSpaceVertex(VertexPipelineInput input)
{
    const Drawable d = GetDrawable();
    
    Vertex vert;
    vert.Position = input.Position;
    vert.Texcoord = input.Texcoord;
    vert.Normal = input.Normal;
    vert.Tangent = input.Tangent;

    const float4x4 modelToWorld = d.ModelToWorld;

    vert.Position = mul(float4(vert.Position, 1.0f), modelToWorld).xyz;
    vert.Normal = mul(vert.Normal, (float3x3)modelToWorld); // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vert.Tangent.xyz = mul(vert.Tangent.xyz, (float3x3)modelToWorld);

    return vert;
}

#endif // PIPELINE_H