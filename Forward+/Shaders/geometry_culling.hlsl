#include "shared_definitions.h"
#include "scene.h"

#define UINT_BIT_SIZE 32u

#ifdef GEO_CULLING

#include "util.h"
#include "culling.h"

struct CullingStats
{
	uint TotalDrawables;
	uint VisibleDrawables;

	uint TotalTriangles;
	uint VisibleTriangles;
};

cbuffer Constants : register(b0)
{
	Camera MainCamera;
	ViewFrustum Frustum;

}

cbuffer PushConstants : register(b128)
{
	uint DrawableCount;
	uint HzbWidth;
	uint HzbHeight;
}

StructuredBuffer<Drawable> Drawables : register(t0);
Texture2D<float> DepthTexture : register(t1);
StructuredBuffer<Mesh> Meshes : register(t2);

SamplerState s_DepthSampler : register(s0);

RWByteAddressBuffer VisibilityMask : register(u0);
RWStructuredBuffer<CullingStats> StatsBuffer : register(u1);

// https://zeux.io/2023/01/12/approximate-projected-bounds/
// Returns false if box is colliding with near plane
bool TryProjectBoxOnScreen(float3 c, float r, float znear, float P00, float P11, out float4 aabb)
{
	if (c.z < r + znear)
		return false;

	float3 cr = c * r;
	float czr2 = c.z * c.z - r * r;

	float vx = sqrt(c.x * c.x + czr2);
	float minx = (vx * c.x - cr.z) / (vx * c.z + cr.x);
	float maxx = (vx * c.x + cr.z) / (vx * c.z - cr.x);

	float vy = sqrt(c.y * c.y + czr2);
	float miny = (vy * c.y - cr.z) / (vy * c.z + cr.y);
	float maxy = (vy * c.y + cr.z) / (vy * c.z - cr.y);

	aabb = float4(minx * P00, miny * P11, maxx * P00, maxy * P11);
	aabb = aabb.xwzy * float4(0.5f, -0.5f, 0.5f, -0.5f) + 0.5f; // clip space -> uv space

	return true;
}

bool IsVisible(const Drawable d)
{
	// Frustum culling
	if (!IsInViewFrustum(d.BoundingVolume, Frustum))
		return false;

#ifdef OCCLUSION_CULLING
	BoundingSphere bsView = d.BoundingVolume;
	bsView.Center = mul(float4(bsView.Center,1.0f), MainCamera.WorldToView).xyz;
	
	float4 aabb;
	const bool shouldDoOcclusionCulling = TryProjectBoxOnScreen(bsView.Center, bsView.Radius, MainCamera.ZNear, MainCamera.ViewToClip[0][0], MainCamera.ViewToClip[1][1], aabb);
	if (shouldDoOcclusionCulling)
	{
		// BoundingSphere nearest point
		const float3 bsNP = bsView.Center - float3(0.0f, 0.0f, bsView.Radius);
		const float4 bsNPClip = mul(float4(bsNP, 1.0f), MainCamera.ViewToClip);
		const float bsNPDepth = GetDepthFromClipPosition(bsNPClip);

		const float bsWidth = (aabb.z - aabb.x) * HzbWidth;
		const float bsHeight = (aabb.w - aabb.y) * HzbHeight;
		const uint hzbMip = floor(log2(max(bsWidth, bsHeight)));

		const float2 hzbUV = 0.5f * (aabb.xy + aabb.zw);
		const float hzbDepth = DepthTexture.SampleLevel(s_DepthSampler, hzbUV, hzbMip);

		if (hzbDepth < bsNPDepth)
			return false;
	}
#endif // OCCLUSION_CULLING

	return true;
}

// Wavefront size is less than UINT_BIT_SIZE so we can't use wave intrinsics to write to visibility buffer
#ifndef WAVEFRONT_SIZE_GE_32
groupshared uint gs_VisibilityMask[OPT_COMP_TG_SIZE / UINT_BIT_SIZE];
#endif

[numthreads(OPT_COMP_TG_SIZE, 1, 1)]
void CS(uint3 threadID : SV_DispatchThreadID, uint3 localThreadID : SV_GroupThreadID)
{
	const uint drawableIndex = threadID.x;
	bool isVisible = false;

	if (drawableIndex < DrawableCount)
	{
		const Drawable d = Drawables[drawableIndex];

#ifdef FORCE_VISIBLE
		isVisible = true;
#else
		isVisible = IsVisible(d);
#endif // FORCE_VISIBLE
	}

	// Write result
#ifdef WAVEFRONT_SIZE_GE_32
	const uint4 visibilityMask = WaveActiveBallot(isVisible);
	if (WaveGetLaneIndex() % UINT_BIT_SIZE == 0)
	{
		const uint writeIndex = drawableIndex / UINT_BIT_SIZE;
		const uint ballotIndex = WaveGetLaneIndex() / UINT_BIT_SIZE;
		VisibilityMask.Store(writeIndex * sizeof(uint), visibilityMask[ballotIndex]);
	}
#else
	const uint ballotIndex = localThreadID.x / UINT_BIT_SIZE;
	const uint localBallotindex = localThreadID.x % UINT_BIT_SIZE;
	gs_VisibilityMask[ballotIndex] = 0;
	GroupMemoryBarrierWithGroupSync();
	if (isVisible)
	{
		const uint visibilityMask = 1u << localBallotindex;
		InterlockedOr(gs_VisibilityMask[ballotIndex], visibilityMask);
	}
	GroupMemoryBarrierWithGroupSync();
	if (localBallotindex == 0)
	{
		const uint writeIndex = drawableIndex / UINT_BIT_SIZE;
		VisibilityMask.Store(writeIndex * sizeof(uint), gs_VisibilityMask[ballotIndex]);
	}
#endif

	// STATS
	if (drawableIndex < DrawableCount)
	{
		const Drawable d = Drawables[drawableIndex];
		const Mesh m = Meshes[d.MeshIndex];
		const uint triangleCount = m.IndexCount / 3;

		const uint totalDrawables = WaveActiveSum(1);
		const uint visibleDrawables = WaveActiveSum(isVisible ? 1 : 0);

		const uint totalTriangles = WaveActiveSum(triangleCount);
		const uint visibleTriangles = WaveActiveSum(isVisible ? triangleCount : 0);

		if (WaveIsFirstLane())
		{
			InterlockedAdd(StatsBuffer[0].TotalDrawables, totalDrawables);
			InterlockedAdd(StatsBuffer[0].VisibleDrawables, visibleDrawables);
			InterlockedAdd(StatsBuffer[0].TotalTriangles, totalTriangles);
			InterlockedAdd(StatsBuffer[0].VisibleTriangles, visibleTriangles);
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
ByteAddressBuffer VisibilityMask : register(t2);

RWStructuredBuffer<IndirectArguments> IndArgs : register(u0);
RWStructuredBuffer<uint> IndArgsCount : register(u1);

[numthreads(OPT_COMP_TG_SIZE, 1, 1)]
void CS(uint3 threadID : SV_DispatchThreadID)
{
	const uint drawableIndex = threadID.x;
	if (drawableIndex >= DrawableCount) return;

#ifdef DRAW_ALL
	const bool isVisible = true;
#else
	const uint visMaskIndex = drawableIndex / UINT_BIT_SIZE;
	const uint visMask = VisibilityMask.Load(visMaskIndex * sizeof(uint));
	const uint drawableVisMaskIndex = drawableIndex % UINT_BIT_SIZE;
	const bool isVisible = visMask & (1u << drawableVisMaskIndex);
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