#include "TextureLoading.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "Render/Commands.h"
#include "Render/RenderAPI.h"
#include "Render/Resource.h"
#include "Render/Texture.h"

namespace TextureLoading
{
	static unsigned char INVALID_TEXTURE_COLOR[] = { 0xff, 0x00, 0x33, 0xff };

	static void* LoadTexture(const std::string& path, int& width, int& height, int& bpp)
	{
		void* data = stbi_load(path.c_str(), &width, &height, &bpp, 4);

		if (!data)
		{
			std::cout << "Warning: Failed to load texture: " << path << std::endl;
			data = INVALID_TEXTURE_COLOR;
			width = 1;
			height = 1;
			bpp = 4;
		}

		return data;
	}

	static void* LoadTextureF(const std::string& path, int& width, int& height, int& bpp)
	{
		void* data = stbi_loadf(path.c_str(), &width, &height, &bpp, 4);

		if (!data)
		{
			std::cout << "Warning: Failed to load texture: " << path << std::endl;
			data = INVALID_TEXTURE_COLOR;
			width = 1;
			height = 1;
			bpp = 4;
		}

		return data;
	}

	static void FreeTexture(void* data)
	{
		if (data != INVALID_TEXTURE_COLOR)
			stbi_image_free(data);
	}

	Texture* LoadTextureHDR(GraphicsContext& context, const std::string& path, RCF creationFlags)
	{
		static constexpr DXGI_FORMAT TEXTURE_FORMAT = DXGI_FORMAT_R32G32B32A32_FLOAT;
		int width, height, bpp;
		void* texData = LoadTextureF(path, width, height, bpp);
		ResourceInitData initData = { &context, texData };
		Texture* texture = GFX::CreateTexture(width, height, creationFlags, 1, TEXTURE_FORMAT, &initData);
		FreeTexture(texData);
		return texture;
	}

	Texture* LoadTexture(GraphicsContext& context, const std::string& path, RCF creationFlags, uint32_t numMips)
	{
		static constexpr DXGI_FORMAT TEXTURE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

		int width, height, bpp;
		void* texData = LoadTexture(path, width, height, bpp);
		Texture* texture;
		if (numMips == 1)
		{
			ResourceInitData initData = { &context, texData };
			texture = GFX::CreateTexture(width, height, creationFlags, numMips, TEXTURE_FORMAT, &initData);
		}
		else
		{
			const uint32_t maxWH = MAX(width, height);
			while (maxWH >> (numMips-1) == 0) numMips--;

			texture = GFX::CreateTexture(width, height, creationFlags, numMips, TEXTURE_FORMAT);
			GFX::Cmd::UploadToTexture(context, texData, texture, 0);
			GFX::Cmd::GenerateMips(context, texture);
		}
		FreeTexture(texData);
		return texture;
	}

	Texture* LoadCubemap(GraphicsContext& context, const std::string& path, RCF creationFlags)
	{
		// Load tex
		static constexpr DXGI_FORMAT TEXTURE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
		int width, height, bpp;
		void* texData = LoadTexture(path, width, height, bpp);

		// Prepare init data
		std::vector<ResourceInitData*> initData;
		ResourceInitData datas[6];
		initData.resize(6);
		uint32_t byteSizePerImg = width * height * bpp / 6;
		uint8_t* bytePtr = (uint8_t*)texData;
		for (size_t i = 0; i < 6; i++)
		{
			datas[i] = { &context, (const void*)(bytePtr + i * byteSizePerImg) };
			initData[i] = &datas[i];
		}

		// Create tex
		Texture* texture = GFX::CreateTextureArray(width, height / 6, 6, creationFlags, 1, TEXTURE_FORMAT, initData);
		FreeTexture(texData);
		return texture;
	}
}