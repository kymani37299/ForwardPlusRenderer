#include "Buffer.h"

#include "Render/Device.h"
#include "Render/Resource.h"

namespace GFX
{
	BufferID CreateBuffer(uint64_t byteSize, uint64_t elementStride, uint64_t creationFlags, const void* initData)
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
		bufferDesc.Usage = GetUsageFlags(creationFlags);
		bufferDesc.BindFlags = GetBindFlags(creationFlags);
		bufferDesc.CPUAccessFlags = GetCPUAccessFlags(creationFlags);
		bufferDesc.MiscFlags = GetMiscFlags(creationFlags);
		bufferDesc.StructureByteStride = buffer.ElementStride;

		API_CALL(Device::Get()->GetHandle()->CreateBuffer(&bufferDesc, subresourceData, buffer.Handle.GetAddressOf()));

		if (creationFlags & RCF_Bind_SB)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.ElementOffset = 0;
			srvDesc.Buffer.ElementWidth = buffer.ElementStride;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = buffer.ByteSize / buffer.ElementStride;

			API_CALL(Device::Get()->GetHandle()->CreateShaderResourceView(buffer.Handle.Get(), &srvDesc, buffer.SRV.GetAddressOf()));
		}

		return id;
	}

	ID3D11Buffer* DX_Buffer(BufferID bufferID)
	{
		const Buffer& buffer = Storage::GetBuffer(bufferID);
		ASSERT(buffer.Handle.Get(), "[DX_GetBuffer] Null resource");
		return buffer.Handle.Get();
	}

	ID3D11ShaderResourceView* DX_SRV(BufferID bufferID)
	{
		const Buffer& buffer = Storage::GetBuffer(bufferID);
		ASSERT(buffer.SRV.Get(), "[DX_GetBuffer] Null resource");
		return buffer.SRV.Get();
	}

	uint64_t GetNumElements(BufferID bufferID)
	{
		const Buffer& buffer = Storage::GetBuffer(bufferID);
		return buffer.ByteSize / buffer.ElementStride;
	}
}