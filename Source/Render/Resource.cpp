#include "Resource.h"

#include <array>
#include <stack>
#include <mutex>

#include "Render/RenderAPI.h"
#include "Render/Shader.h"

namespace GFX
{
	namespace
	{
		template<typename ResourceHandle, uint32_t StorageSize>
		class ResourceStorage
		{
		public:
			ResourceHandle& Allocate(uint32_t& id)
			{
				m_Mutex.lock();
				if (!m_AvailableElements.empty())
				{
					id = m_AvailableElements.top();
					m_AvailableElements.pop();
				}
				else
				{
					ASSERT(m_NextAlloc < StorageSize, "[ResourceStorage] Memory overflow, initialize storage with more memory!");
					id = m_NextAlloc++;
				}
				ResourceHandle& handle = m_Storage[id];
				m_Mutex.unlock();
				return handle;
			}

			void Free(uint32_t id)
			{
				m_Mutex.lock();
				m_AvailableElements.push(id);
				m_Storage[id] = ResourceHandle{};
				m_Mutex.unlock();
			}

			void Clear()
			{
				m_Mutex.lock();
				for (ResourceHandle& handle : m_Storage) handle = ResourceHandle{};
				while (!m_AvailableElements.empty()) m_AvailableElements.pop();
				m_Mutex.unlock();
			}

			ResourceHandle& operator[](uint32_t id)
			{
				return m_Storage[id];
			}

		private:
			std::mutex m_Mutex;
			uint32_t m_NextAlloc = 0;
			std::array<ResourceHandle, StorageSize> m_Storage;
			std::stack<uint32_t> m_AvailableElements;
		};

		ResourceStorage<Buffer, 1024> BufferStorage;
		ResourceStorage<Texture, 1024> TextureStorage;
		ResourceStorage<Shader, 1024> ShaderStorage;
	}

	namespace Storage
	{
		const Buffer& GetBuffer(BufferID id) { return BufferStorage[id.ID]; }
		const Texture& GetTexture(TextureID id) { return TextureStorage[id.ID]; }
		const Shader& GetShader(ShaderID id) { return ShaderStorage[id.ID]; }

		Buffer& CreateBuffer(BufferID& id) { return BufferStorage.Allocate(id.ID); }
		Texture& CreateTexture(TextureID& id) { return TextureStorage.Allocate(id.ID); }
		Shader& CreateShader(ShaderID& id) { return ShaderStorage.Allocate(id.ID); }

		void Free(BufferID& id)
		{
			ASSERT(id.Valid(), "[Storage::Free] Trying to free invalid resource");
			BufferStorage.Free(id.ID);
			id = BufferID{};
		}
		
		void Free(TextureID& id)
		{
			ASSERT(id.Valid(), "[Storage::Free] Trying to free invalid resource");
			TextureStorage.Free(id.ID);
			id = TextureID{};
		}
		
		void Free(ShaderID& id)
		{
			ASSERT(id.Valid(), "[Storage::Free] Trying to free invalid resource");
			ShaderStorage.Free(id.ID);
			id = ShaderID{};
		}

		void ReloadAllShaders()
		{
			// TODO:
		}

		void ClearStorage()
		{
			BufferStorage.Clear();
			TextureStorage.Clear();
			ShaderStorage.Clear();
		}
	}
}