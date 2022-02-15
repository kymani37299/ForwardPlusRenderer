#include "Renderer.h"

#include "System/ApplicationConfiguration.h"
#include "Render/Device.h"
#include "Render/Resource.h"
#include "Render/Texture.h"
#include "Render/Commands.h"
#include "Utility/StringUtility.h"

Renderer::Renderer()
{
	Device::Init();
	m_FinalRT = GFX::CreateRenderTarget(AppConfig.WindowWidth, AppConfig.WindowHeight, true);
}

Renderer::~Renderer()
{
	ID3D11DeviceContext1* context = Device::Get()->GetContext();
	for (RenderPass* renderPass : m_Schedule)
	{
		renderPass->OnDestroy(context);
		delete renderPass;
	}
	GFX::Storage::ClearStorage();
	Device::Destroy();
}

void Renderer::Update(float dt)
{
	ID3D11DeviceContext1* context = Device::Get()->GetContext();
	for (RenderPass* renderPass : m_Schedule)
	{
		renderPass->OnUpdate(context, dt);
	}

	if (AppConfig.WindowSizeDirty)
	{
		// TODO: Delete old RT
		m_FinalRT = GFX::CreateRenderTarget(AppConfig.WindowWidth, AppConfig.WindowHeight, true);
		Device::Get()->CreateSwapchain();

		for (RenderPass* renderPass : m_Schedule)
		{
			renderPass->OnWindowResize(context);
		}

		AppConfig.WindowSizeDirty = false;
	}
}

void Renderer::Render()
{
	ID3D11DeviceContext1* context = Device::Get()->GetContext();
	GFX::Cmd::MarkerBegin(context, "Frame");
	GFX::Cmd::ResetContext(context);
	GFX::Cmd::ClearRenderTarget(context, m_FinalRT);
	GFX::Cmd::BindRenderTarget(context, m_FinalRT);
	for (RenderPass* renderPass : m_Schedule)
	{
		GFX::Cmd::MarkerBegin(context, renderPass->GetPassName());
		renderPass->OnDraw(context);
		GFX::Cmd::MarkerEnd(context);
	}
	Device::Get()->Present(m_FinalRT);
	GFX::Cmd::MarkerEnd(context);
}

void Renderer::AddRenderPass(RenderPass* renderPass)
{ 
	ID3D11DeviceContext1* context = Device::Get()->GetContext();
	renderPass->OnInit(context);
	m_Schedule.push_back(renderPass); 
}