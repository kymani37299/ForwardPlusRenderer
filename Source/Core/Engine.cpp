#include "Engine.h"

#include <sstream>

#include "App/Application.h"
#include "Core/SceneGraph.h"
#include "Render/Device.h"
#include "Loading/LoadingThread.h"
#include "Gui/GUI.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"
#include "System/Input.h"

ApplicationConfiguration AppConfig;
LoadingThread* LoadingThread::s_Instance = nullptr;
PoisonPillTask* PoisonPillTask::s_Instance = nullptr;

Engine::Engine(Application* app)
{
	Window::Init();
	Window::Get()->ShowCursor(false);
	Device::Init();
	LoadingThread::Init();
	GUI::Init();
	m_Application = app;
	m_Application->OnInit(Device::Get()->GetContext());
}

Engine::~Engine()
{
	ID3D11DeviceContext* context = Device::Get()->GetContext();
	m_Application->OnDestroy(context);
	GUI::Destroy();
	delete m_Application;
	LoadingThread::Destroy();
	Window::Destroy();
	GFX::Storage::ClearStorage();
	Device::Destroy();
}

void Engine::Run()
{
	while (Window::Get()->IsRunning())
	{
		const float dt = m_FrameTimer.GetTimeMS();
		ID3D11DeviceContext* context = Device::Get()->GetContext();

		m_FrameTimer.Start();
		WindowInput::InputFrameBegin();

		// Update frame time title
		char* title = (char*)calloc(15, sizeof(char));
		snprintf(title, 15, " (%.2f ms)", dt);
		Window::Get()->AddToTitle(std::string(title));
		free(title);

		// Update
		Window::Get()->Update(dt);
		
		m_Application->OnUpdate(context, dt);
		GUI::Get()->Update(dt);

		// Update window size if needed
		if (AppConfig.WindowSizeDirty)
		{
			Device::Get()->CreateSwapchain();
			m_Application->OnWindowResize(context);
			AppConfig.WindowSizeDirty = false;
		}

		// Render
		GFX::Cmd::ResetContext(context);
		GFX::Cmd::MarkerBegin(context, "Frame");
		
		TextureID finalRT = m_Application->OnDraw(context);
		GUI::Get()->Render(context);
		Device::Get()->EndFrame(finalRT);
		GFX::Cmd::MarkerEnd(context);
		
		WindowInput::InputFrameEnd();
		m_FrameTimer.Stop();
	}
}

void Engine::ReloadShaders()
{
	GFX::Storage::ReloadAllShaders();
	m_Application->OnShaderReload(Device::Get()->GetContext());
}