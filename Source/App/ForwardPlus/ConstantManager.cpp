#include "ConstantManager.h"

#include "Render/Commands.h"
#include "Render/Resource.h"
#include "Render/Buffer.h"

ConstantManager CBManager;

ConstantManager::~ConstantManager()
{
	for (auto& it : m_Buffers) 
	{
		GFX::Storage::Free(it.second);
	}
}

void ConstantManager::Bind(ID3D11DeviceContext* context)
{
	m_Context = context;
}

void ConstantManager::AddInternal(const uint8_t* data, uint32_t stride, bool update)
{
	if (m_DataSize + stride > m_Data.size()) m_Data.resize(m_DataSize + stride);
	uint8_t* dst = &m_Data[m_DataSize];
	memcpy(dst, data, stride);
	m_DataSize += stride;

	if (update)
	{
		uint32_t bufferSize = (m_DataSize + 255) & ~255;
		if (!m_Buffers.contains(bufferSize))
		{
			m_Buffers[bufferSize] = GFX::CreateBuffer(bufferSize, bufferSize, RCF_Bind_CB | RCF_CPU_Write);
		}
		GFX::Cmd::UploadToBuffer(m_Context, m_Buffers[bufferSize], 0, m_Data.data(), 0, bufferSize);
		GFX::Cmd::BindCBV<VS | PS | CS>(m_Context, m_Buffers[bufferSize], 0);
	}
}
