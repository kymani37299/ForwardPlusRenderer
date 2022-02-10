#include "Renderer.h"

#include "Render/Device.h"
#include "Render/Resource.h"

Renderer::Renderer()
{
	Device::Init();
}

Renderer::~Renderer()
{
	ID3D11DeviceContext1* context = Device::Get()->GetContext();
	for (RenderPass* renderPass : m_Schedule)
	{
		renderPass->OnDestroy(context);
		delete renderPass;
	}
	GFX::ClearStorage();
	Device::Destroy();
}

void Renderer::Update(float dt)
{
	ID3D11DeviceContext1* context = Device::Get()->GetContext();
	for (RenderPass* renderPass : m_Schedule)
	{
		renderPass->OnUpdate(context, dt);
	}
}

void Renderer::Render()
{
	ID3D11DeviceContext1* context = Device::Get()->GetContext();
	for (RenderPass* renderPass : m_Schedule)
	{
		renderPass->OnDraw(context);
	}
	Device::Get()->Present();
}