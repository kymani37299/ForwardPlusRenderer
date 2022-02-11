#pragma once

#include <vector>

#include "Common.h"
#include "Render/ResourceID.h"

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
	Float3 Position;
	Float3 Forward;
};

struct SceneGraph
{
	Camera MainCamera;
	std::vector<Entity> Entities;
};