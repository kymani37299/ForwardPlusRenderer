#pragma once

#include <vector>

#include "Common.h"
#include "Render/ResourceID.h"
#include "Utility/Multithreading.h"

struct ID3D11DeviceContext;

struct Material
{
	void UpdateBuffer(ID3D11DeviceContext* context);

	bool UseBlend = false;
	bool UseAlphaDiscard = false;
	Float3 AlbedoFactor = { 1.0f, 1.0f, 1.0f };
	Float3 FresnelR0 = { 0.05f, 0.05f, 0.05f };
	float MetallicFactor = 1.0f;
	float RoughnessFactor = 1.0f;

	BufferID MaterialParams;
	TextureID Albedo;
	TextureID MetallicRoughness;
	TextureID Normal;
};

struct Mesh
{
	BufferID Position; // Float3
	BufferID UV; // Float2
	BufferID Normal; // Float3
	BufferID Tangent; // Float4
	BufferID Indices; // uint16_t
};

struct Drawable
{
	Material Material;
	Mesh Mesh;
};

struct Entity
{
	void UpdateBuffer(ID3D11DeviceContext* context);

	Float3 Position = Float3(0.0f, 0.0f, 0.0f);
	Float3 Scale = Float3(1.0f, 1.0f, 1.0f);

	MTR::MutexVector<Drawable> Drawables;
	BufferID EntityBuffer;
};

struct Camera
{
	static void RotToAxis(Float3 rot, Float3& forward, Float3& up, Float3& right);

	Camera(Float3 position, Float3 rotation, float fov);
	void UpdateBuffer(ID3D11DeviceContext* context);

	float FOV;
	Float3 Position;
	Float3 Rotation; // (Pitch, Yaw, Roll)

	DirectX::XMMATRIX WorldToView;

	BufferID CameraBuffer;
};

enum LightType : uint32_t
{
	LT_Invalid = 0,
	LT_Directional = 1,
	LT_Point = 2,
	LT_Spot = 3,
	LT_Ambient = 4,
};

struct Light
{
	static Light CreateDirectional(Float3 direction, Float3 color);
	static Light CreateAmbient(Float3 color);
	static Light CreatePoint(Float3 position, Float3 color, Float2 falloff);

	LightType Type = LT_Invalid;
	Float3 Position = { 0.0f, 0.0f, 0.0f };		// Point
	Float3 Strength = { 0.0f, 0.0f, 0.0f };		// Dir/Spot/Point/Ambient
	Float2 Falloff = { 0.0f, 0.0f };			// Point/Spot (Start, End)
	Float3 Direction = { 0.0f, 0.0f, 0.0f };	// Dir/Spot
	float SpotPower = 0.0f;						// Spot
};

struct SceneGraph
{
	void UpdateRenderData(ID3D11DeviceContext* context);

	Camera MainCamera{ {0.0f, 2.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 75.0f };
	std::vector<Entity> Entities;
	std::vector<Light> Lights;

	BufferID LightsBuffer;
	BufferID ShadowTransformBuffer;
	TextureID ShadowMapTexture;
};

extern SceneGraph MainSceneGraph;