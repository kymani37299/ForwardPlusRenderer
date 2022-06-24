#include "Buffer.h"

#include "Render/Commands.h"
#include "Render/Device.h"
#include "Render/Resource.h"

namespace GFX
{
	void CreateBufferResources(Buffer& buffer, const void* initData)
	{
		D3D11_SUBRESOURCE_DATA* subresourceData = nullptr;
		if (initData)
		{
			subresourceData = new D3D11_SUBRESOURCE_DATA();
			subresourceData->pSysMem = initData;
			subresourceData->SysMemPitch = buffer.ByteSize;
		}

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = buffer.ByteSize;
		bufferDesc.Usage = GetUsageFlags(buffer.CreationFlags);
		bufferDesc.BindFlags = GetBindFlags(buffer.CreationFlags);
		bufferDesc.CPUAccessFlags = GetCPUAccessFlags(buffer.CreationFlags);
		bufferDesc.MiscFlags = GetMiscFlags(buffer.CreationFlags);
		bufferDesc.StructureByteStride = buffer.ElementStride;

		API_CALL(Device::Get()->GetHandle()->CreateBuffer(&bufferDesc, subresourceData, buffer.Handle.GetAddressOf()));

		if (buffer.CreationFlags & RCF_Bind_SB)
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

		if (buffer.CreationFlags & RCF_Bind_UAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = buffer.ByteSize / buffer.ElementStride;
			uavDesc.Buffer.Flags = 0;
			API_CALL(Device::Get()->GetHandle()->CreateUnorderedAccessView(buffer.Handle.Get(), &uavDesc, buffer.UAV.GetAddressOf()));
		}
	}

	BufferID CreateBuffer(uint32_t byteSize, uint32_t elementStride, uint64_t creationFlags, const void* initData)
	{
		BufferID id;
		Buffer& buffer = Storage::CreateBuffer(id);
		buffer.ByteSize = byteSize;
		buffer.ElementStride = elementStride;
		buffer.CreationFlags = creationFlags;
		CreateBufferResources(buffer, initData);
		return id;
	}

	void ResizeBuffer(ID3D11DeviceContext* context, BufferID bufferID, uint32_t byteSize)
	{
		Buffer& buffer = const_cast<Buffer&>(Storage::GetBuffer(bufferID));
		if (buffer.ByteSize >= byteSize) return;

		uint32_t copySize = MIN(buffer.ByteSize, byteSize);
		ComPtr<ID3D11Resource> oldResource = buffer.Handle;

		buffer.Handle.Reset();
		buffer.SRV.Reset();
		buffer.UAV.Reset();

		buffer.ByteSize = byteSize;
		buffer.CreationFlags |= RCF_CopyDest; // TODO: Add warning if RCF_CopyDest isn't already on buffer
		CreateBufferResources(buffer, nullptr);

		D3D11_BOX sourceRegion;
		sourceRegion.left = 0;
		sourceRegion.right = copySize;
		sourceRegion.top = 0;
		sourceRegion.bottom = 1;
		sourceRegion.front = 0;
		sourceRegion.back = 1;
		context->CopySubresourceRegion(buffer.Handle.Get(), 0, 0, 0, 0, oldResource.Get(), 0, &sourceRegion);
	}

	uint32_t GetNumElements(BufferID bufferID)
	{
		const Buffer& buffer = Storage::GetBuffer(bufferID);
		return buffer.ByteSize / buffer.ElementStride;
	}
}