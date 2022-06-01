#pragma once

#include "Render/ResourceID.h"

struct ID3D11DeviceContext;

class ShadowRenderer
{
public:
	void Init(ID3D11DeviceContext* context);
	TextureID CalculateShadowMask(ID3D11DeviceContext* context, TextureID depth);
	void ReloadTextureResources(ID3D11DeviceContext* context);

private:
	ShaderID m_ShadowmapShader;
	ShaderID m_ShadowmaskShader;

	TextureID m_Shadowmap;
	TextureID m_Shadowmask;
};