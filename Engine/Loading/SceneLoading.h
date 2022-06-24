#pragma once

#include "Core/SceneGraph.h"

namespace SceneLoading
{
	struct LoadedObject
	{
		RenderGroupType RenderGroup;
		uint32_t MeshIndex;
		uint32_t MaterialIndex;
		BoundingSphere BoundingVolume;
		uint32_t EntityIndex;
	};

	struct LoadedScene
	{
		std::vector<LoadedObject> Objects;
		std::vector<Entity> Entities;
	};

	void AddDraws(LoadedScene scene, Entity entity);
	LoadedScene Load(const std::string& path);
}