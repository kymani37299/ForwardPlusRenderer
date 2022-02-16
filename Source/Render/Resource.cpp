#include "Resource.h"

#include <vector>
#include <mutex>

#include "Render/RenderAPI.h"
#include "Render/Shader.h"

namespace GFX
{
	namespace
	{
		template<typename ResourceID, typename ResourceHandle>
		class ResourceStorage
		{
		public:
			ResourceHandle& Allocate(ResourceID& id)
			{
				m_Mutex.lock();
				id = m_Storage.size();
				m_Storage.resize(id + 1);
				m_Mutex.unlock();
				return m_Storage[id];
			}

			uint32_t Size()
			{
				return m_Storage.size();
			}

			void Clear()
			{
				m_Mutex.lock();
				m_Storage.clear();
				m_Mutex.unlock();
			}

			ResourceHandle& operator[](ResourceID id)
			{
				ASSERT(m_Storage.size() > id, "[ResourceStorage] Invalid ID");
				return m_Storage[id];
			}

		private:
			std::mutex m_Mutex;
			std::vector<ResourceHandle> m_Storage;
		};

		ResourceStorage<BufferID, Buffer> BufferStorage;
		ResourceStorage<TextureID, Texture> TextureStorage;
		ResourceStorage<ShaderID, Shader> ShaderStorage;
	}

	namespace Storage
	{
		const Buffer& GetBuffer(BufferID id) { return BufferStorage[id]; }
		const Texture& GetTexture(TextureID id) { return TextureStorage[id]; }
		const Shader& GetShader(ShaderID id) { return ShaderStorage[id]; }

		Buffer& CreateBuffer(BufferID& id) { return BufferStorage.Allocate(id); }
		Texture& CreateTexture(TextureID& id) { return TextureStorage.Allocate(id); }
		Shader& CreateShader(ShaderID& id) { return ShaderStorage.Allocate(id); }

		void ReloadAllShaders()
		{
			for (ShaderID id = 0; id < ShaderStorage.Size(); id++)
			{
				ReloadShader(id);
			}
		}

		void ClearStorage()
		{
			BufferStorage.Clear();
			TextureStorage.Clear();
			ShaderStorage.Clear();
		}
	}
}