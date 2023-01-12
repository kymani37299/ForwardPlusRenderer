#include "shared_definitions.h"
#include "scene.h"

static uint MASK_SIZE = 32;

#ifdef GEO_CULLING

#include "util.h"
#include "culling.h"

cbuffer Constants : register(b0)
{
	Camera MainCamera;
	ViewFrustum Frustum;
	uint DrawableCount;
}

StructuredBuffer<Drawable> Drawables : register(t0);
Texture2D<float> DepthTexture : register(t1);

RWStructuredBuffer<uint> VisibilityMask : register(u0);
RWStructuredBuffer<uint> StatsBuffer : register(u1);

bool IsVisible(const Drawable d, ViewFrustum frustum, Camera camera, Texture2D<float> depthTexture)
{
	// Frustum culling
	if (!IsInViewFrustum(d.BoundingVolume, frustum))
	{
		return false;
	}

#ifndef DISABLE_OCCLUSION_CULLING
	// TODO: Add this as a param
	// NOT_WORKING
	// const uint depthTextureSize = uint2(1024, 768);
	// 
	// // Occlusion culling
	// const float3 cameraDirection = camera.Position - e.BoundingVolume.Center;
	// const float cameraDistance = length(cameraDirection);
	// if (e.BoundingVolume.Radius < cameraDistance)
	// {
	// 	const float3 closestPoint = cameraDirection * e.BoundingVolume.Radius + e.BoundingVolume.Center;
	// 	const float4 closestPointClip = GetClipPos(closestPoint, camera);
	// 	const float2 depthUV = GetUVFromClipPosition(closestPointClip);
	// 
	// 	const bool inDepthMap = depthUV.x < 1.0f && depthUV.x > 0.0f && depthUV.y < 1.0f && depthUV.y > 0.0f;
	// 	if (!inDepthMap)
	// 	{
	// 		return false;
	// 	}
	// 
	// 	const uint3 depthPixelCoord = uint3(depthUV * depthTextureSize, 0);
	// 	const float depth = depthTexture.Load(depthPixelCoord);
	// 	if (closestPointClip.z > depth)
	// 	{
	// 		return false;
	// 	}
	// }
#endif // DISABLE_OCCLUSION_CULLING

	return true;
}

[numthreads(WAVESIZE, 1, 1)]
void CS(uint3 threadID : SV_DispatchThreadID)
{
	const uint drawableIndex = threadID.x;
	if (drawableIndex >= DrawableCount) return;

#ifdef FORCE_VISIBLE
	const bool isVisible = true;
#else
	const Drawable d = Drawables[drawableIndex];
	const bool isVisible = IsVisible(d, Frustum, MainCamera, DepthTexture);
#endif // FORCE_VISIBLE

	VisibilityMask[drawableIndex] = isVisible ? 1 : 0;

	// STATS
	{
		const uint visibleCount = WaveActiveSum(isVisible ? 1 : 0);
		if (WaveIsFirstLane())
		{
			InterlockedAdd(StatsBuffer[0], visibleCount);
		}
	}
}

#endif // GEO_CULLING

#ifdef PREPARE_ARGUMENTS

struct IndirectArguments
{
	uint DrawableID;
	uint IndexCountPerInstance;
	uint InstanceCount;
	uint StartIndexLocation;
	uint BaseVertexLocation;
	uint StartInstanceLocation;
};

cbuffer Constants : register(b0)
{
	uint DrawableCount;
}

StructuredBuffer<Drawable> Drawables : register(t0);
StructuredBuffer<Mesh> Meshes : register(t1);
StructuredBuffer<bool> VisibilityMask : register(t2);

RWStructuredBuffer<IndirectArguments> IndArgs : register(u0);
RWStructuredBuffer<uint> IndArgsCount : register(u1);

[numthreads(WAVESIZE, 1, 1)]
void CS(uint3 threadID : SV_DispatchThreadID)
{
	const uint drawableIndex = threadID.x;
	if (drawableIndex >= DrawableCount) return;

#ifdef DRAW_ALL
	const bool isVisible = true;
#else
	const bool isVisible = VisibilityMask[drawableIndex] == 1;
#endif // DRAW_ALL

	if (isVisible)
	{
		const Drawable d = Drawables[drawableIndex];
		const Mesh m = Meshes[d.MeshIndex];

		IndirectArguments args;
		args.IndexCountPerInstance = m.IndexCount;
		args.InstanceCount = 1;
		args.StartIndexLocation = m.IndexOffset;
		args.BaseVertexLocation = m.VertexOffset;
		args.StartInstanceLocation = 0;
		args.DrawableID = drawableIndex;
		
		uint baseOffset;
		const uint visibleCount = WaveActiveSum(1);
		if (WaveIsFirstLane())
		{
			InterlockedAdd(IndArgsCount[0], visibleCount, baseOffset);
		}
		baseOffset = WaveReadLaneFirst(baseOffset);
		const uint argSlot = baseOffset + WavePrefixSum(1);
		IndArgs[argSlot] = args;
	}
}

#endif // PREPARE_ARGUMENTS