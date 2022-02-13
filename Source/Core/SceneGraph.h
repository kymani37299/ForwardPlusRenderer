#pragma once

#include <vector>

#include "Common.h"
#include "Render/ResourceID.h"

struct ID3D11DeviceContext1;

struct Material
{
	bool UseBlend = false;
	TextureID Albedo = TextureID_Invalid;
};

struct Mesh
{
	BufferID Position = BufferID_Invalid; // Float3
	BufferID UV = BufferID_Invalid; // Float2
	BufferID Normal = BufferID_Invalid; // Float3
	BufferID Tangent = BufferID_Invalid; // Float4
	BufferID Indices = BufferID_Invalid; // uint16_t
};

struct Drawable
{
	Material Material;
	Mesh Mesh;
};

struct Entity
{
	std::vector<Drawable> Drawables;
};

struct Camera
{
	Camera(Float3 position, Float3 Forward, float fov);
	void UpdateBuffer(ID3D11DeviceContext1* context);

	float FOV;
	Float3 Position;
	Float3 Forward;

	BufferID CameraBuffer = BufferID_Invalid;
};

struct SceneGraph
{
	Camera MainCamera{ {0.0f, 2.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 75.0f };
	std::vector<Entity> Entities;
};