#pragma once

#include "Core/RenderPass.h"
#include "Render/ResourceID.h"

class ScenePrepareRenderPass : public RenderPass
{
public:
	virtual std::string GetPassName() const { return "Scene Prepare"; }

	void OnInit(ID3D11DeviceContext* context) override;
	void OnDraw(ID3D11DeviceContext* context) override;
};

class SkyboxRenderPass : public RenderPass
{
public:
	virtual std::string GetPassName() const { return "Skybox"; }

	void OnInit(ID3D11DeviceContext* context) override;
	void OnDraw(ID3D11DeviceContext* context) override;
	void OnShaderReload(ID3D11DeviceContext* context) override;

private:
	ShaderID m_Shader;
	TextureID m_SkyboxCubemap;
};

class ShadowMapRenderPass : public RenderPass
{
public:
	virtual std::string GetPassName() const { return "Shadowmap"; }

	void OnInit(ID3D11DeviceContext* context) override;
	void OnDraw(ID3D11DeviceContext* context) override;

private:
	ShaderID m_Shader;
	BufferID m_TransformBuffer;
};

class DepthPrepassRenderPass : public RenderPass
{
public:
	virtual std::string GetPassName() const { return "Depth Prepass"; }

	void OnInit(ID3D11DeviceContext* context) override;
	void OnDraw(ID3D11DeviceContext* context) override;

private:
	ShaderID m_Shader;
};

class GeometryRenderPass : public RenderPass
{
public:
	virtual std::string GetPassName() const { return "Geometry"; }

	void OnInit(ID3D11DeviceContext* context) override;
	void OnDraw(ID3D11DeviceContext* context) override;

private:
	ShaderID m_Shader;
	ShaderID m_AlphaDiscardShader;
};