#include "Renderer.h"

#include "Render/Device.h"

Renderer::Renderer()
{
	Device::Init();
}

Renderer::~Renderer()
{
	Device::Destroy();
}

void Renderer::Update(float dt)
{

}

void Renderer::Render()
{
	Device::Get()->Present();
}