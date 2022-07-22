#pragma once

#include <Engine/Common.h>
#include <Engine/Render/RenderThread.h>

#include "Scene/SceneGraph.h"

struct GraphicsContext;

class TextureLoadingTask : public RenderTask
{
public:
	TextureLoadingTask(const std::string& texturePath, TextureStorage::Allocation alloc, TextureStorage& textureStorage, ColorUNORM defaultColor = {0.0f, 0.0f, 0.0f, 0.0f}) :
		m_TexturePath(texturePath),
		m_Allocation(alloc),
		m_TextureStorage(textureStorage),
		m_DefaultColor(defaultColor) {}

	void Run(GraphicsContext& context) override;

private:
	std::string m_TexturePath;
	TextureStorage::Allocation m_Allocation;
	TextureStorage& m_TextureStorage;
	ColorUNORM m_DefaultColor;
};