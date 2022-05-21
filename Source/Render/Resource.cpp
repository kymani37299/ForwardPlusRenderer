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
			void Lock()
			{
				m_Mutex.lock();
			}

			void Unlock()
			{
				m_Mutex.unlock();
			}

			ResourceHandle& Allocate(uint32_t& id)
			{
				Lock();
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
				Unlock();
				return handle;
			}

			void Free(uint32_t id)
			{
				Lock();
				m_AvailableElements.push(id);
				m_Storage[id] = ResourceHandle{};
				Unlock();
			}

			void Clear()
			{
				Lock();
				for (ResourceHandle& handle : m_Storage) handle = ResourceHandle{};
				while (!m_AvailableElements.empty()) m_AvailableElements.pop();
				Unlock();
			}

			uint32_t Size()
			{
				return StorageSize;
			}

			// Note: Not thread safe!
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
		const Buffer& GetBuffer(BufferID id) 
		{
			ASSERT(id.Valid(), "[Storage::GetBuffer] Trying to get invalid resource");
			return BufferStorage[id.ID]; 
		}

		const Texture& GetTexture(TextureID id) 
		{ 
			ASSERT(id.Valid(), "[Storage::GetTexture] Trying to get invalid resource");
			return TextureStorage[id.ID]; 
		}

		const Shader& GetShader(ShaderID id) 
		{
			ASSERT(id.Valid(), "[Storage::GetShader] Trying to get invalid resource");
			return ShaderStorage[id.ID]; 
		}

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
			ShaderStorage.Lock();
			for (uint32_t i = 0; i < ShaderStorage.Size(); i++)
			{
				if (ShaderStorage[i].CreationFlags != 0)
				{
					ShaderStorage[i].Implementations.clear();
				}
			}
			ShaderStorage.Unlock();
		}

		void ClearStorage()
		{
			BufferStorage.Clear();
			TextureStorage.Clear();
			ShaderStorage.Clear();
		}
	}
}