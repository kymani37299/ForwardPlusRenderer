#pragma once

#include "Core/RenderPass.h"
#include "Render/ResourceID.h"

class ScenePrepareRenderPass : public RenderPass
{
public:
	virtual std::string GetPassName() const { return "Scene Prepare"; }

	void OnInit(ID3D11DeviceContext1* context) override;
	void OnDraw(ID3D11DeviceContext1* context) override;
};

class DepthPrepassRenderPass : public RenderPass
{
public:
	virtual std::string GetPassName() const { return "Depth Prepass"; }

	void OnInit(ID3D11DeviceContext1* context) override;
	void OnDraw(ID3D11DeviceContext1* context) override;

private:
	ShaderID m_Shader;
};

class GeometryRenderPass : public RenderPass
{
public:
	virtual std::string GetPassName() const { return "Geometry"; }

	void OnInit(ID3D11DeviceContext1* context) override;
	void OnDraw(ID3D11DeviceContext1* context) override;

private:
	ShaderID m_Shader;
};