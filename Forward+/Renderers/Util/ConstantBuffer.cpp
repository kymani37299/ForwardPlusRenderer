#include "ConstantBuffer.h"

#include <unordered_map>

#include <Engine/Render/Commands.h>
#include <Engine/Render/Buffer.h>

Buffer* ConstantBuffer::GetBuffer(GraphicsContext& context)
{
	uint32_t bufferSize = (m_DataSize + 255) & ~255;
	Buffer* buffer = GFX::CreateBuffer(bufferSize, bufferSize, RCF::CBV | RCF::CPU_Access);
	GFX::SetDebugName(buffer, "ConstantBuffer");
	GFX::Cmd::UploadToBufferImmediate(buffer, 0, m_Data.data(), 0, m_DataSize);
	GFX::Cmd::Delete(context, buffer);
	return buffer;
}

void ConstantBuffer::AddInternal(const uint8_t* data, uint32_t stride)
{
	if (m_DataSize + stride > m_Data.size()) m_Data.resize(m_DataSize + stride);
	uint8_t* dst = &m_Data[m_DataSize];
	memcpy(dst, data, stride);
	m_DataSize += stride;
}
