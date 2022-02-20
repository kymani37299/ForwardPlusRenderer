#include "Renderer.h"

#include "System/ApplicationConfiguration.h"
#include "Render/Device.h"
#include "Render/Resource.h"
#include "Render/Texture.h"
#include "Render/Commands.h"
#include "Utility/StringUtility.h"

TextureID FinalRT_Color;
TextureID FinalRT_DepthStencil;

static void UpdateFinalRT()
{
	if (FinalRT_Color.Valid())
	{
		GFX::Storage::Free(FinalRT_Color);
		GFX::Storage::Free(FinalRT_DepthStencil);
	}
	FinalRT_Color = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV);
	FinalRT_DepthStencil = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_DSV);
}

Renderer::Renderer()
{
	Device::Init();
	UpdateFinalRT();
}

Renderer::~Renderer()
{
	ID3D11DeviceContext* context = Device::Get()->GetContext();
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
	ID3D11DeviceContext* context = Device::Get()->GetContext();
	for (RenderPass* renderPass : m_Schedule)
	{
		renderPass->OnUpdate(context, dt);
	}

	if (AppConfig.WindowSizeDirty)
	{
		UpdateFinalRT();
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
	ID3D11DeviceContext* context = Device::Get()->GetContext();
	GFX::Cmd::MarkerBegin(context, "Frame");
	GFX::Cmd::ResetContext(context);
	GFX::Cmd::ClearRenderTarget(context, FinalRT_Color, FinalRT_DepthStencil);
	GFX::Cmd::BindRenderTarget(context, FinalRT_Color, FinalRT_DepthStencil);
	for (RenderPass* renderPass : m_Schedule)
	{
		GFX::Cmd::MarkerBegin(context, renderPass->GetPassName());
		renderPass->OnDraw(context);
		GFX::Cmd::MarkerEnd(context);
	}
	Device::Get()->EndFrame(FinalRT_Color);
	GFX::Cmd::MarkerEnd(context);
}

void Renderer::AddRenderPass(RenderPass* renderPass)
{ 
	ID3D11DeviceContext* context = Device::Get()->GetContext();
	renderPass->OnInit(context);
	m_Schedule.push_back(renderPass); 
}

void Renderer::OnShaderReload()
{
	ID3D11DeviceContext* context = Device::Get()->GetContext();
	for (RenderPass* renderPass : m_Schedule)
	{
		renderPass->OnShaderReload(context);
	}
}