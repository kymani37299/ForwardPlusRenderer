#pragma once

#include "Common.h"
#include "Render/RenderThread.h"
#include "Core/SceneGraph.h"

class TextureLoadingTask : public RenderTask
{
public:
	TextureLoadingTask(const std::string& texturePath, TextureStorage::Allocation alloc, TextureStorage& textureStorage, ColorUNORM defaultColor = {0.0f, 0.0f, 0.0f, 0.0f}) :
		m_TexturePath(texturePath),
		m_Allocation(alloc),
		m_TextureStorage(textureStorage),
		m_DefaultColor(defaultColor) {}

	void Run(ID3D11DeviceContext* context) override;

private:
	std::string m_TexturePath;
	TextureStorage::Allocation m_Allocation;
	TextureStorage& m_TextureStorage;
	ColorUNORM m_DefaultColor;
};