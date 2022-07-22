#include "SamplerManager.h"

#include <Engine/Render/Commands.h>

SamplerManager SSManager;

void SamplerManager::Bind(GraphicsState& state)
{
	uint32_t slot = 0;
	
	// s_LinearWrap
	GFX::Cmd::BindSampler(state, slot++, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	// s_AnisoWrap
	GFX::Cmd::BindSampler(state, slot++, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_FILTER_ANISOTROPIC);

	// s_PointWrap
	GFX::Cmd::BindSampler(state, slot++, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_FILTER_MIN_MAG_MIP_POINT);

	// s_PointBorder
	GFX::Cmd::BindSampler(state, slot++, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_FILTER_MIN_MAG_MIP_POINT);

	// s_LinearBorder
	GFX::Cmd::BindSampler(state, slot++, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	// s_PointClamp
	GFX::Cmd::BindSampler(state, slot++, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_FILTER_MIN_MAG_MIP_POINT);
}
