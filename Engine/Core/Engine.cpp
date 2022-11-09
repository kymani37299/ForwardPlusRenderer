#include "Engine.h"

#include <sstream>

#include "Core/Application.h"
#include "Render/Commands.h"
#include "Render/Device.h"
#include "Render/Memory.h"
#include "Render/Texture.h"
#include "Render/Shader.h"
#include "Render/RenderThread.h"
#include "Gui/GUI.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"
#include "System/Input.h"

struct Texture;

ApplicationConfiguration AppConfig;

Engine::Engine(Application* app)
{
	Window::Init();
	Window::Get()->ShowCursor(false);
	Device::Init();
	GFX::InitShaderCompiler();
	RenderThreadPool::Init(8);
	GUI::Init();
	m_Application = app;
	m_Application->OnInit(Device::Get()->GetContext());
	GFX::Cmd::SubmitContext(Device::Get()->GetContext());
}

Engine::~Engine()
{
	GraphicsContext& context = Device::Get()->GetContext();
	GFX::Cmd::FlushContext(context);

	m_Application->OnDestroy(context);
	GUI::Destroy();
	delete m_Application;
	RenderThreadPool::Destroy();
	GFX::DestroyShaderCompiler();
	Device::Destroy();
	Window::Destroy();
}

void Engine::Run()
{
	while (Window::Get()->IsRunning())
	{
		const float dt = m_FrameTimer.GetTimeMS();
		GraphicsContext& context = Device::Get()->GetContext();

		m_FrameTimer.Start();
		WindowInput::InputFrameBegin();

		// Update
		Window::Get()->Update(dt);
		
		m_Application->OnUpdate(context, dt);
		GUI::Get()->Update(dt);

		// Graphics frame init
		GFX::Cmd::FlushContext(context);
		GFX::Cmd::ResetContext(context);
		DeferredTrash::Get()->Clear();

		// Update window size if needed
		if (AppConfig.WindowSizeDirty)
		{
			Device::Get()->RecreateSwapchain();
			m_Application->OnWindowResize(context);
			AppConfig.WindowSizeDirty = false;
		}

		// Render
		Texture* finalRT = m_Application->OnDraw(context);
		if (!finalRT)
		{
			finalRT = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV);
			DeferredTrash::Get()->Put(finalRT);
		}
		
		GUI::Get()->Render(context, finalRT); // TODO: First copy the finalRT to the texture with good format then render gui to that texture
		Device::Get()->EndFrame(finalRT);

		WindowInput::InputFrameEnd();
		m_FrameTimer.Stop();
	}
}

void Engine::ReloadShaders()
{
	GFX::ReloadAllShaders();
	m_Application->OnShaderReload(Device::Get()->GetContext());
}