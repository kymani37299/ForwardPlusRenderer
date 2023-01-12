#ifndef SCENE_H
#define SCENE_H

#include "culling.h"

struct DirectionalLight
{
	float3 Direction;
	float Padding0;
	float3 Radiance;
	float Padding1;
};

struct SceneInfo
{
	uint NumLights;
	float3 Padding1;

	DirectionalLight DirLight;

	float3 AmbientRadiance;
	float Padding2;

	float2 ScreenSize;
	float2 Padding3;
	float AspectRatio;
};

struct Camera
{
	float4x4 WorldToView;
	float4x4 ViewToClip;
	float4x4 ClipToWorld;
	float3 Position;
	float Padding;
	float2 Jitter;
};

struct Drawable
{
	uint MaterialIndex;
	uint MeshIndex;

	float4x4 ModelToWorld;
	BoundingSphere BoundingVolume;
};

struct Mesh
{
	uint VertexOffset;
	uint IndexOffset;
	uint IndexCount;
};

struct Vertex
{
	float3 Position;
	float2 Texcoord;
	float3 Normal;
	float4 Tangent;
};

struct Material
{
	float3 AlbedoFactor;
	float3 FresnelR0;
	float MetallicFactor;
	float RoughnessFactor;
	uint Albedo;
	uint MetallicRoughness;
	uint Normal;
};

#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_POINT 2
#define LIGHT_TYPE_SPOT 3
#define LIGHT_TYPE_AMBIENT 4

struct Light
{
	bool IsSpot;
	float3 Position;
	float3 Radiance;
	float2 Falloff;		// (Start, End)
	
	// Spot only
	float3 Direction;
	float SpotPower;
};

#endif // SCENE_H