#include "Resource.h"

#include <vector>
#include <atomic>

#include "Render/RenderAPI.h"
#include "Render/Shader.h"

namespace GFX
{
	namespace
	{
		template<typename ResourceHandle>
		class ResourceStorage
		{
		public:
			ResourceStorage(uint32_t size):
				m_Size(size)
			{
				m_Storage.resize(m_Size);
			}

			ResourceHandle& Allocate(uint32_t& id)
			{
				id = m_NextAlloc++;
				ASSERT(id < m_Size, "[ResourceStorage] Memory overflow!");
				return m_Storage[id];
			}

			uint32_t Size() const { return m_Size; }

			void Clear() // Note: Not threadsafe
			{
				m_Storage.clear();
				m_NextAlloc.store(0);
			}

			ResourceHandle& operator[](uint32_t id)
			{
				ASSERT(m_Size > m_NextAlloc.load(), "[ResourceStorage] Invalid ID");
				return m_Storage[id];
			}

		private:
			uint32_t m_Size;
			std::atomic<uint32_t> m_NextAlloc;
			std::vector<ResourceHandle> m_Storage;
		};

		ResourceStorage<Buffer> BufferStorage{ 1024 };
		ResourceStorage<Texture> TextureStorage{ 1024 };
		ResourceStorage<Shader> ShaderStorage{ 1024 };
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
			std::atomic<uint32_t> m_NextAlloc;
			m_NextAlloc.fetch_add(1);
			m_NextAlloc++;

			BufferStorage.Clear();
			TextureStorage.Clear();
			ShaderStorage.Clear();
		}
	}
}