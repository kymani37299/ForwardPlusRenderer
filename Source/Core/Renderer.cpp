#include "Renderer.h"

#include "Render/Device.h"
#include "Render/Resource.h"

Renderer::Renderer()
{
	Device::Init();
}

Renderer::~Renderer()
{
	GFX::ClearStorage();
	Device::Destroy();
}

void Renderer::Update(float dt)
{

}

void Renderer::Render()
{
	Device::Get()->Present();
}