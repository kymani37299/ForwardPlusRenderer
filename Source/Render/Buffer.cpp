#include "Buffer.h"

#include "Render/Device.h"

namespace GFX
{
	BufferID CreateVertexBuffer(uint64_t byteSize, uint64_t elementStride, void* initData)
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
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		API_CALL(Device::Get()->GetHandle()->CreateBuffer(&bufferDesc, subresourceData, buffer.Handle.GetAddressOf()));

		return id;
	}
}