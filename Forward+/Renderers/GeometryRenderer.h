#pragma once

#include <Engine/Render/ResourceID.h>

struct ID3D11DeviceContext;

class GeometryRenderer
{
public:
	void Init(ID3D11DeviceContext* context);
	void DepthPrepass(ID3D11DeviceContext* context);
	void Draw(ID3D11DeviceContext* context, TextureID shadowMask, BufferID visibleLights, TextureID irradianceMap, TextureID ambientOcclusion);

private:
	ShaderID m_DepthPrepassShader;
	ShaderID m_GeometryShader;
};