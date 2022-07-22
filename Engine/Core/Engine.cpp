#include "Engine.h"

#include <sstream>

#include "Core/Application.h"
#include "Render/Commands.h"
#include "Render/Device.h"
#include "Render/Memory.h"
#include "Render/Texture.h"
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
	ReleaseContextCache();
	RenderThreadPool::Destroy();
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
		DeferredTrash::Clear();

		// Update window size if needed
		if (AppConfig.WindowSizeDirty)
		{
			Device::Get()->RecreateSwapchain();
			m_Application->OnWindowResize(context);
			AppConfig.WindowSizeDirty = false;
		}

		// Render
		GFX::Cmd::MarkerBegin(context, "Frame");
		
		Texture* finalRT = m_Application->OnDraw(context);
		if (!finalRT)
		{
			finalRT = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV);
			DeferredTrash::Put(finalRT);
		}

		GUI::Get()->Render(context, finalRT);
		Device::Get()->EndFrame(finalRT);
		GFX::Cmd::MarkerEnd(context);

		WindowInput::InputFrameEnd();
		m_FrameTimer.Stop();
	}
}

// TODO: Integrate this
void Engine::ReloadShaders()
{
	// GFX::Storage::ReloadAllShaders();
	// m_Application->OnShaderReload(Device::Get()->GetContext());
}