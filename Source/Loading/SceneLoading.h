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
	};

	using LoadedScene = std::vector<LoadedObject>;

	void AddDraws(LoadedScene scene, uint32_t entityIndex);
	LoadedScene Load(const std::string& path, bool inBackground = false);
}