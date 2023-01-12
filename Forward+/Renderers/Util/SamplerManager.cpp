#include "SamplerManager.h"

#include <Engine/Render/Commands.h>

SamplerManager SSManager;

void SamplerManager::Bind(GraphicsState& state)
{
	state.Table.SMPs = BindVector<Sampler>{};
	
	// s_LinearWrap
	state.Table.SMPs[0] = {D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_WRAP};
	
	// s_AnisoWrap
	state.Table.SMPs[1] = {D3D12_FILTER_ANISOTROPIC , D3D12_TEXTURE_ADDRESS_MODE_WRAP};

	// s_PointWrap
	state.Table.SMPs[2] = {D3D12_FILTER_MIN_MAG_MIP_POINT , D3D12_TEXTURE_ADDRESS_MODE_WRAP};

	// s_PointBorder
	state.Table.SMPs[3] ={D3D12_FILTER_MIN_MAG_MIP_POINT , D3D12_TEXTURE_ADDRESS_MODE_BORDER};

	// s_LinearBorder
	state.Table.SMPs[4] = {D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_BORDER};

	// s_PointClamp
	state.Table.SMPs[5] = { D3D12_FILTER_MIN_MAG_MIP_POINT , D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
}
