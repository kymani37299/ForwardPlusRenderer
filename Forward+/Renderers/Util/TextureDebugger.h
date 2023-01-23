#pragma once

#include <string>
#include <vector>

struct Texture;

class TextureDebugger
{
public:
	struct DebugTexture
	{
		std::string Name;
		Texture* Tex;

		bool ShowAlpha = false;
		int SelectedMip = 0;
		float RangeMin = 0.0f;
		float RangeMax = 1.0f;
		bool FindRange = false;
	};

	void AddTexture(const std::string& name, Texture* texture)
	{
		for (auto& debugTex : m_Textures)
		{
			if (debugTex.Name == name)
			{
				debugTex.Tex = texture;
				return;
			}
		}

		m_Textures.push_back({ name, texture });
	}

	const std::vector<DebugTexture>& GetTextures() const
	{
		return m_Textures;
	}

	DebugTexture& GetSelectedTexture()
	{
		return m_Textures[m_SelectedTexture];
	}

	void SetSelectedTexture(uint32_t selectedTexture)
	{
		m_SelectedTexture = selectedTexture;
	}

	// Returns imgui handle for preview texture
	const void* GetPreviewTextureHandle()
	{
		return m_PreviewTextureHandle;
	}

	void SetPreviewTextureHandle(const void* previewTextureHandle)
	{
		m_PreviewTextureHandle = previewTextureHandle;
	}

	bool& GetEnabledRef()
	{
		return m_Enabled;
	}

private:
	bool m_Enabled = false;
	uint32_t m_SelectedTexture = 0;
	std::vector<DebugTexture> m_Textures;
	const void* m_PreviewTextureHandle = nullptr;
};

extern TextureDebugger TexDebugger;