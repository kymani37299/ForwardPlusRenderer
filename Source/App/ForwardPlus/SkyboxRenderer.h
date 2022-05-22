#pragma once
#include "Render/ResourceID.h"

struct ID3D11DeviceContext;

class SkyboxRenderer
{
public:
	SkyboxRenderer(const std::string& texturePath): m_SkyboxTexturePath(texturePath) {}

	void Init(ID3D11DeviceContext* context);
	void Draw(ID3D11DeviceContext* context);

	void OnShaderReload(ID3D11DeviceContext* context);

private:
	std::string m_SkyboxTexturePath;

	BufferID m_CubeVB;
	ShaderID m_SkyboxShader;
	TextureID m_SkyboxCubemap;
};