#include <Windows.h>
#include <iostream>
#include <time.h>

#include "Common.h"
#include "Core/Renderer.h"
#include "Core/SceneGraph.h"
#include "Render/Commands.h"
#include "Render/Texture.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/Device.h"
#include "Render/PipelineState.h"
#include "Loading/SceneLoading.h"
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
		srand((uint32_t)time(0));
	}

	~AppScope()
	{
		Window::Destroy();
	}
};

// [0,1]
float Rand()
{
	return (float) rand() / RAND_MAX;
}

// [-1,1]
float Rand2()
{
	return Rand() * 2.0f - 1.0f;
}

SceneGraph MainSceneGraph;

class GeometryRenderPass : public RenderPass
{
public:
	void OnInit(ID3D11DeviceContext1* context) override
	{
		MainSceneGraph.Lights.push_back(Light::CreateAmbient(Float3(0.1f, 0.1f, 0.15f)));
		MainSceneGraph.Lights.push_back(Light::CreateDirectional(Float3(-1.0f, -1.0f, -1.0f), Float3(0.2f, 0.2f, 0.23f)));

		constexpr uint32_t NUM_LIGHTS = 5000;
		for (uint32_t i = 0; i < NUM_LIGHTS; i++)
		{
			Float3 position = Float3(200.0f, 100.0f, 200.0f) * Float3(Rand2(), Rand2(), Rand2());
			Float3 color = Float3(Rand(), Rand(), Rand());
			Float2 falloff = Float2(1.0f + 3.0f * Rand(), 5.0f + 10.0f * Rand());
			MainSceneGraph.Lights.push_back(Light::CreatePoint(position, color, falloff));
		}
		
		Entity sponza = SceneLoading::LoadEntity("Resources/sponza/sponza.gltf");
		sponza.Scale *= 0.1f;
		MainSceneGraph.Entities.push_back(std::move(sponza));
		//MainSceneGraph.Entities.push_back(SceneLoading::LoadEntity("Resources/cube/cube.gltf"));
		MainSceneGraph.UpdateRenderData(context);

		m_Shader = GFX::CreateShader("Source/Shaders/geometry.hlsl");
	}

	void OnDraw(ID3D11DeviceContext1* context) override
	{
		MainSceneGraph.MainCamera.UpdateBuffer(context);
		
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;

		GFX::Cmd::SetPipelineState(context, pso);
		GFX::Cmd::BindShader(context, m_Shader, true);
		context->PSSetSamplers(0, GFX::GetStaticSamplersNum(), GFX::GetStaticSamplers());

		{
			ID3D11Buffer* cbv = GFX::DX_GetBuffer(MainSceneGraph.MainCamera.CameraBuffer);
			context->VSSetConstantBuffers(0, 1, &cbv);
			context->PSSetConstantBuffers(0, 1, &cbv);
		}

		{
			ID3D11ShaderResourceView* srv = GFX::DX_GetBufferSRV(MainSceneGraph.LightsBuffer);
			context->PSSetShaderResources(1, 1, &srv);
		}

		for (Entity e : MainSceneGraph.Entities)
		{
			{
				ID3D11Buffer* cbv = GFX::DX_GetBuffer(e.EntityBuffer);
				context->VSSetConstantBuffers(1, 1, &cbv);
			}

			for (Drawable d : e.Drawables)
			{
				Mesh& m = d.Mesh;
				GFX::Cmd::BindVertexBuffers(context, { m.Position, m.UV, m.Normal, m.Tangent });
				GFX::Cmd::BindIndexBuffer(context, m.Indices);
				
				ID3D11ShaderResourceView* srv = GFX::DX_GetTextureSRV(d.Material.Albedo); 
				context->PSSetShaderResources(0, 1, &srv);
				
				context->DrawIndexed(GFX::GetNumBufferElements(m.Indices), 0, 0);
			}
		}
	}

private:
	ShaderID m_Shader;
};

namespace Input
{
	bool IsKeyPressed(unsigned int key)
	{
		return WindowInput::IsKeyPressed(key);
	}

	bool IsKeyJustPressed(unsigned int key)
	{
		return WindowInput::IsKeyJustPressed(key);
	}

	Float2 GetMousePos()
	{
		return WindowInput::GetMousePos();
	}

	Float2 GetMouseDelta()
	{
		return WindowInput::GetMouseDelta();
	}
}

void UpdateInput(float dt)
{
	if (Input::IsKeyJustPressed('R'))
	{
		GFX::Storage::ReloadAllShaders();
	}
	
	const float movement_speed = 0.1f;
	char mov_inputs[] = { 'W', 'S', 'A', 'D', 'Q', 'E'};
	Float3 mov_effects[] = { {1.0f, 0.0f, 0.0f},{-1.0f, 0.0f, 0.0f},{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},{0.0f, -1.0f, 0.0f},{0.0f, 1.0f, 0.0f} };
	static_assert(STATIC_ARRAY_SIZE(mov_inputs) == STATIC_ARRAY_SIZE(mov_effects));
	for (uint16_t i = 0; i < STATIC_ARRAY_SIZE(mov_inputs); i++)
	{
		if (Input::IsKeyPressed(mov_inputs[i]))
			MainSceneGraph.MainCamera.Position += movement_speed * mov_effects[i];
	}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParams, int showFlags)
{
	AppConfig.AppHandle = instance;
	AppConfig.WindowTitle = "Forward plus graphics";
	AppConfig.WindowWidth = 1024;
	AppConfig.WindowHeight = 768;

	AppScope _appScope;
	RedirectToVSConsoleScoped _vsConsoleRedirect;

	Renderer r;
	r.AddRenderPass(new GeometryRenderPass());
	float dt = 0.0f;

	while (Window::Get()->IsRunning())
	{
		WindowInput::InputFrameBegin();
		Window::Get()->Update(dt);
		UpdateInput(dt);
		r.Update(dt);
		r.Render();
		WindowInput::InputFrameEnd();
	}
}