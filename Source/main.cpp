#include <Windows.h>
#include <iostream>

#include "Common.h"
#include "Render/Commands.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/Device.h"
#include "Core/Renderer.h"
#include "System/VSConsoleRedirect.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

ApplicationConfiguration AppConfig;

class AppScope
{
public:
	AppScope()
	{
		Window::Init();
	}

	~AppScope()
	{
		Window::Destroy();
	}
};

class RedRenderPass : public RenderPass
{
public:
	void OnInit(ID3D11DeviceContext1* context) override
	{
		struct FCVert
		{
			Float2 Position;
			Float2 UV;
		};

		const std::vector<FCVert> fcVBData = {
			FCVert{	{1.0,1.0},		{1.0,0.0}},
			FCVert{	{-1.0,-1.0},	{0.0,1.0}},
			FCVert{	{1.0,-1.0},		{1.0,1.0}},
			FCVert{	{1.0,1.0},		{1.0,0.0}},
			FCVert{ {-1.0,1.0},		{0.0,0.0}},
			FCVert{	{-1.0,-1.0},	{0.0,1.0}}
		};

		m_Shader = GFX::CreateShader("Source/Shaders/red.hlsl");
		m_VB = GFX::CreateVertexBuffer(fcVBData.size() * sizeof(FCVert), sizeof(FCVert), fcVBData.data());

		D3D11_DEPTH_STENCIL_DESC dsDesc{};
		dsDesc.DepthEnable = false;
		dsDesc.StencilEnable = false;

		API_CALL(Device::Get()->GetHandle()->CreateDepthStencilState(&dsDesc, m_DepthState.GetAddressOf()));

		D3D11_RASTERIZER_DESC rsDesc{};
		rsDesc.CullMode = D3D11_CULL_NONE;
		rsDesc.FillMode = D3D11_FILL_SOLID;

		API_CALL(Device::Get()->GetHandle()->CreateRasterizerState(&rsDesc, m_RasterizerState.GetAddressOf()));

		D3D11_BLEND_DESC bDesc{};
		bDesc.RenderTarget[0].BlendEnable = false;
		bDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		API_CALL(Device::Get()->GetHandle()->CreateBlendState(&bDesc, m_BlendState.GetAddressOf()));
		
	}

	void OnDraw(ID3D11DeviceContext1* context) override
	{
		const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

		GFX::Cmd::BindShader(context, m_Shader);
		GFX::Cmd::BindVertexBuffer(context, m_VB);
		context->OMSetDepthStencilState(m_DepthState.Get(), 0xff);
		context->OMSetBlendState(m_BlendState.Get(), blendFactor, 0xff);
		context->RSSetState(m_RasterizerState.Get());

		context->Draw(6, 0);
	}

private:
	BufferID m_VB;
	ShaderID m_Shader;
	ComPtr<ID3D11DepthStencilState> m_DepthState;
	ComPtr<ID3D11RasterizerState> m_RasterizerState;
	ComPtr<ID3D11BlendState> m_BlendState;
};

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParams, int showFlags)
{
	AppConfig.AppHandle = instance;
	AppConfig.WindowTitle = "Forward plus graphics";
	AppConfig.WindowWidth = 1024;
	AppConfig.WindowHeight = 768;

	AppScope _appScope;
	RedirectToVSConsoleScoped _vsConsoleRedirect;

	Renderer r;
	r.AddRenderPass(new RedRenderPass());
	float dt = 0.0f;

	while (Window::Get()->IsRunning())
	{
		Window::Get()->Update(dt);
		r.Update(dt);
		r.Render();
	}
}