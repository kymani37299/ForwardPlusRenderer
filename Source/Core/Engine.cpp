#include "Engine.h"

#include "Core/Renderer.h"
#include "Core/SceneGraph.h"
#include "Core/RenderPasses.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"
#include "System/Input.h"

ApplicationConfiguration AppConfig;

Engine::Engine()
{
	Window::Init();
	m_Renderer = new Renderer();
	m_Renderer->AddRenderPass(new GeometryRenderPass());
}

Engine::~Engine()
{
	delete m_Renderer;
	Window::Destroy();
}

void Engine::UpdateInput(float dt)
{
	if (Input::IsKeyJustPressed('R'))
	{
		GFX::Storage::ReloadAllShaders();
	}

	const float movement_speed = 0.1f;
	char mov_inputs[] = { 'W', 'S', 'A', 'D', 'Q', 'E' };
	Float3 mov_effects[] = { {1.0f, 0.0f, 0.0f},{-1.0f, 0.0f, 0.0f},{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},{0.0f, -1.0f, 0.0f},{0.0f, 1.0f, 0.0f} };
	static_assert(STATIC_ARRAY_SIZE(mov_inputs) == STATIC_ARRAY_SIZE(mov_effects));
	for (uint16_t i = 0; i < STATIC_ARRAY_SIZE(mov_inputs); i++)
	{
		if (Input::IsKeyPressed(mov_inputs[i]))
			MainSceneGraph.MainCamera.Position += movement_speed * mov_effects[i];
	}
}

void Engine::Run()
{
	float dt = m_FrameTimer.GetTimeMS();
	while (Window::Get()->IsRunning())
	{
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