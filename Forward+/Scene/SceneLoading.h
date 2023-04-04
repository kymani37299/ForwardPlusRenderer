#pragma once

#include "Scene/SceneGraph.h"

struct GraphicsContext;

namespace SceneLoading
{
	struct LoadedObject
	{
		RenderGroupType RenderGroup;
		uint32_t MeshIndex;
		uint32_t MaterialIndex;
		
		DirectX::XMFLOAT4X4 Transform = XMUtility::ToXMFloat4x4(DirectX::XMMatrixIdentity());
		BoundingSphere BoundingVolume;
	};
	using LoadedScene = std::vector<LoadedObject>;

	// We are using position, scale and rotation from baseDrawable
	void AddDraws(GraphicsContext& context, const LoadedScene scene, const Drawable& baseDrawable);

	LoadedScene Load(GraphicsContext& context, const std::string& path);
}