#include "TextureLoading.h"

#include <Engine/Render/Resource.h>
#include <Engine/Render/Texture.h>
#include <Engine/Loading/TextureLoading.h>

void TextureLoadingTask::Run(GraphicsContext& context)
{
	Texture* tex = nullptr;
	if (!m_TexturePath.empty())
	{
		tex = TextureLoading::LoadTexture(context, m_TexturePath, RCF_None);
	}
	else
	{
		ResourceInitData initData = { &context, &m_DefaultColor };
		tex = GFX::CreateTexture(1, 1, RCF_None, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &initData);
	}
	GFX::SetDebugName(tex, "TextureLoadingTask::UploadTexture");
	DeferredTrash::Put(tex);

	m_TextureStorage.UpdateTexture(context, m_Allocation, tex);
}
