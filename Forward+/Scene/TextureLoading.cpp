#include "TextureLoading.h"

#include <Engine/Render/Resource.h>
#include <Engine/Render/Texture.h>
#include <Engine/Loading/TextureLoading.h>

void TextureLoadingTask::Run(GraphicsContext& context)
{
	constexpr uint32_t NumMips = 6;

	Texture* tex = nullptr;
	if (!m_TexturePath.empty())
	{
		tex = TextureLoading::LoadTexture(context, m_TexturePath, RCF::None, NumMips);
	}
	else
	{
		ResourceInitData initData = { &context, &m_DefaultColor };
		tex = GFX::CreateTexture(1, 1, RCF::None, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &initData);
	}

	m_TextureStorage.UpdateTexture(m_TextureIndex, tex);
}
