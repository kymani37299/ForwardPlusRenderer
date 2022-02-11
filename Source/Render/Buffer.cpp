#include "Buffer.h"

#include "Render/Device.h"
#include "Render/Resource.h"

namespace GFX
{
	BufferID CreateBuffer(uint64_t byteSize, uint64_t elementStride, D3D11_USAGE usageFlags, uint32_t bindFlags, const void* initData = nullptr)
	{
		BufferID id;
		Buffer& buffer = Storage::CreateBuffer(id);
		buffer.ByteSize = byteSize;
		buffer.ElementStride = elementStride;

		D3D11_SUBRESOURCE_DATA* subresourceData = nullptr;
		if (initData)
		{
			subresourceData = new D3D11_SUBRESOURCE_DATA();
			subresourceData->pSysMem = initData;
			subresourceData->SysMemPitch = byteSize;
		}

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = byteSize;
		bufferDesc.Usage = usageFlags;
		bufferDesc.BindFlags = bindFlags;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		API_CALL(Device::Get()->GetHandle()->CreateBuffer(&bufferDesc, subresourceData, buffer.Handle.GetAddressOf()));

		return id;
	}

	BufferID CreateVertexBuffer(uint64_t byteSize, uint64_t elementStride, const void* initData)
	{
		return CreateBuffer(byteSize, elementStride, D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, initData);
	}

	BufferID CreateIndexBuffer(uint64_t byteSize, uint32_t elementStride, const void* initData)
	{
		return CreateBuffer(byteSize, elementStride, D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER, initData);
	}
}