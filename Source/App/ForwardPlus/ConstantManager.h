#pragma once

#include <vector>
#include <unordered_map>

#include "Common.h"
#include "Render/ResourceID.h"
#include "Render/Commands.h"

struct ID3D11DeviceContext;

class ConstantManager
{
public:
	~ConstantManager();
	
	void Clear() { m_DataSize = 0; }
	void Bind(ID3D11DeviceContext* context);

	template<typename T>
	void Add(const T& value, bool update = false)
	{
		const T* valuePtr = &value;
		AddInternal(reinterpret_cast<const uint8_t*>(valuePtr), sizeof(T), update);
	}

private:
	void AddInternal(const uint8_t* data, uint32_t stride, bool update);

private:
	uint32_t m_DataSize = 0;
	std::vector<uint8_t> m_Data;
	std::unordered_map<uint32_t, BufferID> m_Buffers;
	ID3D11DeviceContext* m_Context;
};

extern ConstantManager CBManager;