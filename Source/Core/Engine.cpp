#include "Engine.h"

#include <sstream>

#include "Core/Renderer.h"
#include "Core/SceneGraph.h"
#include "Core/RenderPasses.h"
#include "Loading/LoadingThread.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"
#include "System/Input.h"

ApplicationConfiguration AppConfig;
LoadingThread* LoadingThread::s_Instance = nullptr;
PoisonPillTask* PoisonPillTask::s_Instance = nullptr;

Engine::Engine()
{
	Window::Init();
	Window::Get()->ShowCursor(false);
	m_Renderer = new Renderer();
	LoadingThread::Init();

	m_Renderer->AddRenderPass(new ScenePrepareRenderPass());
	m_Renderer->AddRenderPass(new SkyboxRenderPass());
	m_Renderer->AddRenderPass(new DepthPrepassRenderPass());
	m_Renderer->AddRenderPass(new GeometryRenderPass());
}

Engine::~Engine()
{
	LoadingThread::Destroy();
	delete m_Renderer;
	Window::Destroy();
}

void Engine::UpdateInput(float dt)
{
	float dtSec = dt / 1000.0f;

	if (Input::IsKeyJustPressed('R'))
	{
		GFX::Storage::ReloadAllShaders();
		m_Renderer->OnShaderReload();
	}

	const float movement_speed = 100.0f;
	const float mouse_speed = 1000.0f;
	char mov_inputs[] = { 'W', 'S', 'A', 'D', 'Q', 'E' };
	Float3 mov_effects[] = { {0.0f, 0.0f, 1.0f},{0.0f, 0.0f, -1.0f},{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},{0.0f, -1.0f, 0.0f},{0.0f, 1.0f, 0.0f} };
	static_assert(STATIC_ARRAY_SIZE(mov_inputs) == STATIC_ARRAY_SIZE(mov_effects));

	Float3 moveDir{ 0.0f, 0.0f, 0.0f };
	for (uint16_t i = 0; i < STATIC_ARRAY_SIZE(mov_inputs); i++)
	{
		if (Input::IsKeyPressed(mov_inputs[i]))
			moveDir += dtSec * movement_speed * mov_effects[i];
	}
	
	Float4 moveDir4{ moveDir.x, moveDir.y, moveDir.z, 1.0f };
	Float4 relativeDir = Float4(DirectX::XMVector4Transform(moveDir4.ToXM(), MainSceneGraph.MainCamera.WorldToView));
	MainSceneGraph.MainCamera.Position += Float3(relativeDir.x, relativeDir.y, relativeDir.z);

	Float2 mouseDelta = Input::GetMouseDelta();
	Float3& cameraRot = MainSceneGraph.MainCamera.Rotation;
	cameraRot.y -= dtSec * mouse_speed * mouseDelta.x;
	cameraRot.x -= dtSec * mouse_speed * mouseDelta.y;
	cameraRot.x = std::clamp(cameraRot.x, -1.5f, 1.5f);
}

void Engine::Run()
{
	while (Window::Get()->IsRunning())
	{
		const float dt = m_FrameTimer.GetTimeMS();

		// Update frame time title
		char* title = (char*)calloc(15, sizeof(char));
		snprintf(title, 15, " (%.2f ms)", dt);
		Window::Get()->AddToTitle(std::string(title));
		free(title);

		m_FrameTimer.Start();
		WindowInput::InputFrameBegin();

		Window::Get()->Update(dt);
		UpdateInput(dt);
		m_Renderer->Update(dt);
		m_Renderer->Render();
		
		WindowInput::InputFrameEnd();
		m_FrameTimer.Stop();
	}
}