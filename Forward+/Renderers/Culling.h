#pragma once

#include <Engine/Render/ResourceID.h>

struct ID3D11DeviceContext;

class Culling
{
public:
	void Init(ID3D11DeviceContext* context);
	void UpdateResources(ID3D11DeviceContext* context);

	void CullGeometries(ID3D11DeviceContext* context);
	void CullLights(ID3D11DeviceContext* context, TextureID depth);

	BufferID GetVisibleLightsBuffer() const { return m_VisibleLightsBuffer; }

private:

	// Light culling
	uint32_t m_NumTilesX = 0;
	uint32_t m_NumTilesY = 0;

	ShaderID m_LightCullingShader;
	BufferID m_VisibleLightsBuffer;
};