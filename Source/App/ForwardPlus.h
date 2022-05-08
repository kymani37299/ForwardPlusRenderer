#pragma once

#include "App/Application.h"
#include "Render/ResourceID.h"

class ForwardPlus : public Application
{
public:
	void OnInit(ID3D11DeviceContext* context);
	void OnDestroy(ID3D11DeviceContext* context);

	TextureID OnDraw(ID3D11DeviceContext* context);
	void OnUpdate(ID3D11DeviceContext* context, float dt);

	void OnShaderReload(ID3D11DeviceContext* context);
	void OnWindowResize(ID3D11DeviceContext* context);

private:
	ShaderID m_SkyboxShader;
	ShaderID m_ShadowmapShader;
	ShaderID m_DepthPrepassShader;
	ShaderID m_GeometryShader;
	ShaderID m_GeometryAlphaDiscardShader;

	ShaderID m_ComputeTestShader;

	TextureID m_SkyboxCubemap;
	TextureID m_FinalRT;
	TextureID m_FinalRT_Depth;

	BufferID m_IndexBuffer;

	BitField m_VisibilityMask;
};