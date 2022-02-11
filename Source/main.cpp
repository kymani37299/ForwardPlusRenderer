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
	}

	void OnDraw(ID3D11DeviceContext1* context) override
	{
		PipelineState pipelineState = GFX::DefaultPipelineState();

		GFX::Cmd::BindShader(context, m_Shader);
		GFX::Cmd::BindVertexBuffer(context, m_VB);
		GFX::Cmd::SetPipelineState(context, pipelineState);

		context->Draw(6, 0);
	}

private:
	BufferID m_VB;
	ShaderID m_Shader;
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