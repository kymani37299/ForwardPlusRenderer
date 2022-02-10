#include "Resource.h"

#include <vector>

namespace GFX
{

	namespace
	{
		std::vector<Buffer> BufferStorage;
		std::vector<Texture> TextureStorage;
	}

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

	void ClearStorage()
	{
		BufferStorage.clear();
		TextureStorage.clear();
	}
}