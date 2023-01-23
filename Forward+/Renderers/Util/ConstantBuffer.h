#pragma once

#include <vector>

#include <Engine/Common.h>

struct Buffer;
struct GraphicsContext;

class ConstantBuffer
{
public:
	template<typename T>
	void Add(const T& value)
	{
		const T* valuePtr = &value;
		AddInternal(reinterpret_cast<const uint8_t*>(valuePtr), sizeof(T));

	}

	Buffer* GetBuffer(GraphicsContext& context);

private:
	void AddInternal(const uint8_t* data, uint32_t stride);

private:
	uint32_t m_DataSize = 0;
	std::vector<uint8_t> m_Data;
};