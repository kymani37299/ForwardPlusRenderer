#pragma once

#include <vector>

struct GraphicsState;
struct D3D12_STATIC_SAMPLER_DESC;

class SamplerManager
{
public:
	void Bind(GraphicsState& state);

private:
	std::vector<D3D12_STATIC_SAMPLER_DESC> m_Samplers;
};

extern SamplerManager SSManager;