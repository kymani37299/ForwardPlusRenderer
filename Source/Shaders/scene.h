struct CameraData
{
	float4x4 WorldToView;
	float4x4 ViewToClip;
	float3 Position;
};

struct EntityData
{
	float4x4 ModelToWorld;
};

struct MaterialParams
{
	float3 AlbedoFactor;
	float3 FresnelR0;
	float MetallicFactor;
	float RoughnessFactor;
	uint Albedo;
	uint MetallicRoughness;
	uint Normal;
};

struct Light
{
	uint Type;			// 1 - Dir, 2 - Point, 3 - Spot, 4 - Ambient
	float3 Position;	// Point
	float3 Strength;	// All
	float2 Falloff;		// Point/Spot (Start, End)
	float3 Direction;	// Dir/Spot
	float SpotPower;	// Spot
};