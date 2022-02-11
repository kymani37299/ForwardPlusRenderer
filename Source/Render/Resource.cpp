#include "Resource.h"

#include <vector>

#include "Render/RenderAPI.h"
#include "Render/Shader.h"

namespace GFX
{
	namespace
	{
		std::vector<Buffer> BufferStorage;
		std::vector<Texture> TextureStorage;
		std::vector<Shader> ShaderStorage;
	}

	namespace Storage
	{
		const Buffer& GetBuffer(BufferID id)
		{
			ASSERT(BufferStorage.size() > id, "[GetBuffer] Invalid ID");
			return BufferStorage[id];
		}

		const Texture& GetTexture(TextureID id)
		{
			ASSERT(TextureStorage.size() > id, "[GetTexture] Invalid ID");
			return TextureStorage[id];
		}

		const Shader& GetShader(ShaderID id)
		{
			ASSERT(ShaderStorage.size() > id, "[GetShader] Invalid ID");
			return ShaderStorage[id];
		}

		Buffer& CreateBuffer(BufferID& id)
		{
			id = BufferStorage.size();
			BufferStorage.resize(id + 1);
			return BufferStorage[id];
		}

		Texture& CreateTexture(TextureID& id)
		{
			id = TextureStorage.size();
			TextureStorage.resize(id + 1);
			return TextureStorage[id];
		}

		Shader& CreateShader(ShaderID& id)
		{
			id = ShaderStorage.size();
			ShaderStorage.resize(id + 1);
			return ShaderStorage[id];
		}

		void ReloadAllShaders()
		{
			for (ShaderID id = 0; id < ShaderStorage.size(); id++)
			{
				ReloadShader(id);
			}
		}

		void ClearStorage()
		{
			BufferStorage.clear();
			TextureStorage.clear();
			ShaderStorage.clear();
		}
	}
}