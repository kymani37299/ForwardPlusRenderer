#pragma once

#include "Core/RenderPass.h"
#include "Render/ResourceID.h"

class ScenePrepareRenderPass : public RenderPass
{
public:
	void OnInit(ID3D11DeviceContext1* context) override;
};

class GeometryRenderPass : public RenderPass
{
public:
	void OnInit(ID3D11DeviceContext1* context) override;
	void OnDraw(ID3D11DeviceContext1* context) override;

private:
	ShaderID m_Shader;
};