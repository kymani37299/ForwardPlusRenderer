#include "TextureLoading.h"

#include <Engine/Render/Resource.h>
#include <Engine/Render/Texture.h>

void TextureLoadingTask::Run(ID3D11DeviceContext* context)
{
	TextureID tex;
	if (!m_TexturePath.empty())
	{
		tex = GFX::LoadTexture(context, m_TexturePath, RCF_Bind_SRV);
	}
	else
	{
		tex = GFX::CreateTexture(1, 1, RCF_Bind_SRV, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &m_DefaultColor);
	}

	m_TextureStorage.UpdateTexture(context, m_Allocation, tex);
	GFX::Storage::Free(tex);
}
