#pragma once

#include <vector>

struct ID3D11DeviceContext;
struct ID3D11SamplerState;

class SamplerManager
{
public:
	~SamplerManager();

	void Init();
	void Bind(ID3D11DeviceContext* context);

private:
	std::vector<ID3D11SamplerState*> m_Samplers;
};

extern SamplerManager SSManager;