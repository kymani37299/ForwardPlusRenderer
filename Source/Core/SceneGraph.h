#pragma once

#include <vector>

#include "Common.h"
#include "Render/ResourceID.h"

struct Material
{
	TextureID Albedo;
};

struct Mesh
{
	BufferID Position;
	BufferID Indices;
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

class SceneGraph
{
	Camera MainCamera;
	std::vector<Entity> Entities;
};