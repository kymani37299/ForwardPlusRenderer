#pragma once

#include <vector>

#include "Core/RenderPass.h"

class Renderer
{
public:
	Renderer();
	~Renderer();

	void Update(float dt);
	void Render();

	void AddRenderPass(RenderPass* renderPass) { m_Schedule.push_back(renderPass); }

private:
	std::vector<RenderPass*> m_Schedule;
};