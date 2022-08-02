#ifndef CULLING_H
#define CULLING_H

struct ViewFrustum
{
	float4 Planes[6];
};

struct BoundingSphere
{
	float3 Center;
	float Radius;
};

BoundingSphere CreateBoundingSphere(float3 position, float radius)
{
	BoundingSphere bs;
	bs.Center = position;
	bs.Radius = radius;
	return bs;
}

// p0 p1 p2 must be counterclockwise
float4 CreateFrustumPlane(float3 p0, float3 p1, float3 p2)
{
	const float3 v = p1 - p0;
	const float3 u = p2 - p0;
	const float3 normal = normalize(cross(v, u));
	float4 fp = float4(normal, 0.0f);
	fp.w = -fp.x * p0.x - fp.y * p0.y - fp.z * p0.z;
	return fp;
}

// Points must be in order: ftl ftr fbl fbr ntl ntr nbl nbr
ViewFrustum CreateViewFrustum(float3 points[8])
{
	ViewFrustum vf;

	// Top Bottom Left Right Near Far
	vf.Planes[0] = CreateFrustumPlane(points[5], points[4], points[0]);
	vf.Planes[1] = CreateFrustumPlane(points[6], points[7], points[3]);
	vf.Planes[2] = CreateFrustumPlane(points[4], points[6], points[2]);
	vf.Planes[3] = CreateFrustumPlane(points[7], points[5], points[3]);
	vf.Planes[4] = CreateFrustumPlane(points[4], points[5], points[7]);
	vf.Planes[5] = CreateFrustumPlane(points[1], points[0], points[2]);
	return vf;
}

bool IsInViewFrustum(BoundingSphere bs, ViewFrustum vf)
{
	for (uint i = 0; i < 6; i++)
	{
		const float sd = dot(float4(bs.Center, 1.0f), vf.Planes[i]);
		if (sd < -bs.Radius) return false;
	}
	return true;
}

#endif // CULLING_H
