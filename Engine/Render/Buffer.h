#pragma once

#include "Render/Device.h"
#include "Render/Resource.h"

struct Buffer : public Resource 
{
	uint32_t ByteSize;
	uint32_t Stride;
	D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;
};

namespace GFX
{
	Buffer* CreateBuffer(uint32_t byteSize, uint32_t elementStride, RCF creationFlags, ResourceInitData* initData = nullptr);
	void ResizeBuffer(GraphicsContext& context, Buffer* buffer, uint32_t byteSize);

	inline void ExpandBuffer(GraphicsContext& context, Buffer* buffer, uint32_t byteSize)
	{
		if (byteSize > buffer->ByteSize)
		{
			ResizeBuffer(context, buffer, byteSize);
		}
	}

	inline Buffer* CreateIndexBuffer(uint32_t byteSize, uint32_t elementStride, ResourceInitData* initData)
	{
		return CreateBuffer(byteSize, elementStride, RCF::None, initData);
	}

	template<typename VertexType>
	Buffer* CreateVertexBuffer(uint32_t numElements, ResourceInitData* initData)
	{
		constexpr uint32_t vertStride = sizeof(VertexType);
		return CreateBuffer(vertStride * numElements, vertStride, RCF::None, initData);
	}

	template<typename ConstantType>
	Buffer* CreateConstantBuffer()
	{
		constexpr uint32_t stride = (sizeof(ConstantType) + 255) & ~255;
		return CreateBuffer(stride, stride, RCF::CBV | RCF::CPU_Access);
	}
}

class ReadbackBuffer
{
public:
	ReadbackBuffer(uint32_t stride)
	{
		for (uint32_t i = 0; i < Device::IN_FLIGHT_FRAME_COUNT; i++)
		{
			m_ReadBuffers[i] = GFX::CreateBuffer(stride, stride, RCF::Readback);
			m_WriteBuffers[i] = GFX::CreateBuffer(stride, stride, RCF::UAV);
		}
	}

	~ReadbackBuffer()
	{
		for (uint32_t i = 0; i < Device::IN_FLIGHT_FRAME_COUNT; i++)
		{
			delete m_ReadBuffers[i];
			delete m_WriteBuffers[i];
		}
	}

	Buffer* GetReadBuffer() const { return m_ReadBuffers[m_ReadIndex]; }
	Buffer* GetWriteBuffer() const { return m_WriteBuffers[m_WriteIndex]; }

	void Private_Sync()
	{
		m_ReadIndex = m_WriteIndex;
		m_WriteIndex = (m_WriteIndex + 1) % Device::IN_FLIGHT_FRAME_COUNT;
	}

	Buffer* Private_GetReadBufferForCopy() const { return m_ReadBuffers[m_WriteIndex]; }
	uint32_t GetStride() const { return m_ReadBuffers[0]->Stride; }

private:
	uint32_t m_ReadIndex = 0;
	uint32_t m_WriteIndex = 1;

	Buffer* m_ReadBuffers[Device::IN_FLIGHT_FRAME_COUNT];
	Buffer* m_WriteBuffers[Device::IN_FLIGHT_FRAME_COUNT];
};