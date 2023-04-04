#include "Engine.h"

#include <sstream>

#include "Core/Application.h"
#include "Render/Commands.h"
#include "Render/Device.h"
#include "Render/Texture.h"
#include "Render/Shader.h"
#include "Render/RenderThread.h"
#include "Render/RenderResources.h"
#include "Gui/GUI.h"
#include "Gui/EngineGUI/ShaderCompilerGUI.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"
#include "System/Input.h"

struct Texture;

ApplicationConfiguration AppConfig;

Engine::Engine(Application* app)
{
	PROFILE_SECTION_CPU("Init Engine");

	Window::Init();
	Window::Get()->ShowCursor(false);
	
	Device::Init();
	GFX::InitShaderCompiler();
	RenderThreadPool::Init(8);

	GraphicsContext& context = ContextManager::Get().GetCreationContext();
	ID3D12CommandList* cmdList = context.CmdList.Get();
	OPTICK_GPU_CONTEXT(cmdList);

	GFX::Cmd::BeginRecording(context);
	GFX::InitRenderingResources(context);

	GUI::Init();
	GUI::Get()->AddElement(new ShaderCompilerGUI());

	m_Application = app;
	m_Application->OnInit(context);

	GFX::Cmd::EndRecordingAndSubmit(context);
	GFX::Cmd::WaitToFinish(context);
}

Engine::~Engine()
{
	ContextManager::Get().Flush();
	GraphicsContext& context = ContextManager::Get().GetCreationContext();

	m_Application->OnDestroy(context);
	GUI::Destroy();
	delete m_Application;
	RenderThreadPool::Destroy();
	GFX::DestroyShaderCompiler();
	GFX::DestroyRenderingResources(context);
	Device::Destroy();
	Window::Destroy();
}

void Engine::Run()
{
	while (Window::Get()->IsRunning())
	{
		OPTICK_FRAME("MainThread");

		const float dt = m_FrameTimer.GetTimeMS();

		GraphicsContext& context = ContextManager::Get().NextFrame();

		ID3D12CommandList* cmdList = context.CmdList.Get();
		OPTICK_GPU_CONTEXT(cmdList);

		m_FrameTimer.Start();
		WindowInput::InputFrameBegin();
		
		GFX::Cmd::BeginRecording(context);

		// Update
		Window::Get()->Update(dt);
		{
			PROFILE_SECTION_CPU("Application::Update");
			m_Application->OnUpdate(context, dt);
		}
		GUI::Get()->Update(dt);

		// Update window size if needed
		if (AppConfig.WindowSizeDirty)
		{
			Device::Get()->RecreateSwapchain(context);
			{
				PROFILE_SECTION(context, "Application::WindowResize");
				m_Application->OnWindowResize(context);
			}
			AppConfig.WindowSizeDirty = false;
		}

		// Render
		Texture* finalRT = nullptr;
		{
			PROFILE_SECTION(context, "Application::Draw");
			finalRT = m_Application->OnDraw(context);
		}
		if (!finalRT)
		{
			finalRT = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF::RTV);
			GFX::Cmd::Delete(context, finalRT);
		}
		Device::Get()->CopyToSwapchain(context, finalRT);

		GUI::Get()->Render(context);
		Device::Get()->EndFrame(context);

		WindowInput::InputFrameEnd();
		m_FrameTimer.Stop();
	}
}

void Engine::ReloadShaders()
{
	// TODO: Rework shader reload system this
	// GFX::ReloadAllShaders();
	// m_Application->OnShaderReload(ContextManager::Get().GetContext());
}