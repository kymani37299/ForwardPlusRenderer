struct SceneInfo
{
	uint NumLights;
	float2 ScreenSize;
	float AspectRatio;
};

struct Camera
{
	float4x4 WorldToView;
	float4x4 ViewToClip;
	float3 Position;
	float2 Jitter;
};

struct Drawable
{
	uint EntityIndex;
	uint MaterialIndex;
};

struct Entity
{
	float4x4 ModelToWorld;
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
	uint Type;			// 1 - Dir, 2 - Point, 3 - Spot, 4 - Ambient
	float3 Position;	// Point
	float3 Strength;	// All
	float2 Falloff;		// Point/Spot (Start, End)
	float3 Direction;	// Dir/Spot
	float SpotPower;	// Spot
};