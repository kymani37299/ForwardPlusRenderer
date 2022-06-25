#include "SamplerManager.h"

#include <Engine/Render/Device.h>

SamplerManager SSManager;

SamplerManager::~SamplerManager()
{
	for (ID3D11SamplerState* sampler : m_Samplers)
	{
		sampler->Release();
	}
}

void SamplerManager::Init()
{
	ID3D11Device* device = Device::Get()->GetHandle();

	m_Samplers.resize(6);

	D3D11_SAMPLER_DESC samplerDesc{};
	D3D11_SAMPLER_DESC defaultSamplerDesc{};
	defaultSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	defaultSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	defaultSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	defaultSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	defaultSamplerDesc.MipLODBias = 0;
	defaultSamplerDesc.MaxAnisotropy = 16;
	defaultSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	defaultSamplerDesc.BorderColor[0] = 0.0f;
	defaultSamplerDesc.BorderColor[1] = 0.0f;
	defaultSamplerDesc.BorderColor[2] = 0.0f;
	defaultSamplerDesc.BorderColor[3] = 0.0f;
	defaultSamplerDesc.MinLOD = 0.0f;
	defaultSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// s_LinearWrap
	samplerDesc = defaultSamplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	device->CreateSamplerState(&samplerDesc, &m_Samplers[0]);

	// s_AnisoWrap
	samplerDesc = defaultSamplerDesc;
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	device->CreateSamplerState(&samplerDesc, &m_Samplers[1]);

	// s_PointWrap
	samplerDesc = defaultSamplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	device->CreateSamplerState(&samplerDesc, &m_Samplers[2]);

	// s_PointBorder
	samplerDesc = defaultSamplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	device->CreateSamplerState(&samplerDesc, &m_Samplers[3]);

	// s_LinearBorder
	samplerDesc = defaultSamplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	device->CreateSamplerState(&samplerDesc, &m_Samplers[4]);

	// s_PointClamp
	samplerDesc = defaultSamplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	device->CreateSamplerState(&samplerDesc, &m_Samplers[5]);
}

void SamplerManager::Bind(ID3D11DeviceContext* context)
{
	context->VSSetSamplers(0, m_Samplers.size(), m_Samplers.data());
	context->PSSetSamplers(0, m_Samplers.size(), m_Samplers.data());
	context->CSSetSamplers(0, m_Samplers.size(), m_Samplers.data());
	context->GSSetSamplers(0, m_Samplers.size(), m_Samplers.data());
	context->HSSetSamplers(0, m_Samplers.size(), m_Samplers.data());
	context->DSSetSamplers(0, m_Samplers.size(), m_Samplers.data());
}
