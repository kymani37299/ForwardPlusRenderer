#pragma once

#include <vector>

#include <Engine/Render/ResourceID.h>

struct ID3D11DeviceContext;

class SSAORenderer
{
public:
	void Init(ID3D11DeviceContext* context);
	TextureID Draw(ID3D11DeviceContext* context, TextureID depth);

	void UpdateResources(ID3D11DeviceContext* context);

private:
	std::vector<Float4> m_Kernel;
	TextureID m_NoiseTexture;
	ShaderID m_Shader;
	TextureID m_SSAOSampleTexture;
	TextureID m_SSAOTexture;
	TextureID m_NoSSAOTexture;
};