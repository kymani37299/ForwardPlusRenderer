#pragma once

#include <Engine/Common.h>
#include <Engine/Render/RenderThread.h>

#include "Scene/SceneGraph.h"

struct GraphicsContext;

class TextureLoadingTask : public RenderTask
{
public:
	TextureLoadingTask(const std::string& texturePath, uint32_t textureIndex, TextureStorage& textureStorage, ColorUNORM defaultColor = {0.0f, 0.0f, 0.0f, 0.0f}) :
		m_TexturePath(texturePath),
		m_TextureIndex(textureIndex),
		m_TextureStorage(textureStorage),
		m_DefaultColor(defaultColor) {}

	void Run(GraphicsContext& context) override;

private:
	std::string m_TexturePath;
	uint32_t m_TextureIndex;
	TextureStorage& m_TextureStorage;
	ColorUNORM m_DefaultColor;
};