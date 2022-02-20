#pragma once

#include <vector>

#include "Core/RenderPass.h"
#include "Render/Resource.h"

class Renderer
{
public:
	Renderer();
	~Renderer();

	void Update(float dt);
	void Render();

	void AddRenderPass(RenderPass* renderPass);

	void OnShaderReload();
private:
	std::vector<RenderPass*> m_Schedule;
};

extern TextureID FinalRT_Color;
extern TextureID FinalRT_DepthStencil;