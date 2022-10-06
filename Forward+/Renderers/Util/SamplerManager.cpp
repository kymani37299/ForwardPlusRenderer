#include "SamplerManager.h"

#include <Engine/Render/Commands.h>

SamplerManager SSManager;

void SamplerManager::Bind(GraphicsState& state)
{
	uint32_t slot = 0;
	
	// s_LinearWrap
	state.Table.SMPs.push_back({ D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_WRAP });
	
	// s_AnisoWrap
	state.Table.SMPs.push_back({ D3D12_FILTER_ANISOTROPIC , D3D12_TEXTURE_ADDRESS_MODE_WRAP });

	// s_PointWrap
	state.Table.SMPs.push_back({ D3D12_FILTER_MIN_MAG_MIP_POINT , D3D12_TEXTURE_ADDRESS_MODE_WRAP });

	// s_PointBorder
	state.Table.SMPs.push_back({ D3D12_FILTER_MIN_MAG_MIP_POINT , D3D12_TEXTURE_ADDRESS_MODE_BORDER });

	// s_LinearBorder
	state.Table.SMPs.push_back({ D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_BORDER });

	// s_PointClamp
	state.Table.SMPs.push_back({ D3D12_FILTER_MIN_MAG_MIP_POINT , D3D12_TEXTURE_ADDRESS_MODE_CLAMP });
}
