#include "ForwardPlus.h"

#include <Engine/Common.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Texture.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Device.h>
#include <Engine/Render/Resource.h>
#include <Engine/Gui/GUI.h>
#include <Engine/System/Input.h>
#include <Engine/System/Window.h>
#include <Engine/System/ApplicationConfiguration.h>
#include <Engine/Utility/MathUtility.h>
#include <Engine/Utility/Random.h>

#include "Renderers/Util/ConstantManager.h"
#include "Renderers/Util/SamplerManager.h"
#include "Renderers/Util/VertexPipeline.h"
#include "Gui/GUI_Implementations.h"
#include "Scene/SceneLoading.h"
#include "Scene/SceneGraph.h"
#include "Shaders/shared_definitions.h"

namespace ForwardPlusPrivate
{
	void PrepareScene(ID3D11DeviceContext* context)
	{
		const Float3 dirLight = Float3(-0.2f, -1.0f, -0.2f);

		MainSceneGraph.CreateAmbientLight(context, Float3(0.05f, 0.05f, 0.2f));
		MainSceneGraph.CreateDirectionalLight(context, dirLight, Float3(3.7f, 2.0f, 0.9f));

		if (AppConfig.Settings.contains("SIMPLE_SCENE"))
		{
			Entity plane{};
			plane.Position = { 0.0f, -10.0f, 0.0f };
			plane.Scale = { 10000.0f, 1.0f, 10000.0f };
			SceneLoading::LoadedScene cubeScene = SceneLoading::Load("Resources/cube/cube.gltf");
			SceneLoading::AddDraws(cubeScene, plane);

			constexpr uint32_t NUM_CUBES = 50;
			for (uint32_t i = 0; i < NUM_CUBES; i++)
			{
				const Float3 position = Float3{ Random::SNorm(), Random::SNorm(), Random::SNorm() } *Float3{ 100.0f, 100.0f, 100.0f };
				const Float3 scale = Float3{ Random::Float(0.1f, 10.0f), Random::Float(0.1f, 10.0f) , Random::Float(0.1f, 10.0f) };
				Entity cube{};
				cube.Position = position;
				cube.Scale = scale;
				SceneLoading::AddDraws(cubeScene, cube);
			}
		}
		else
		{
			// Note: Too big for the github, so using low res scene on repo
			//SceneLoading::LoadedScene scene = SceneLoading::Load("Resources/SuperSponza/NewSponza_Main_Blender_glTF.gltf");
			SceneLoading::LoadedScene scene = SceneLoading::Load("Resources/sponza/sponza.gltf");

			constexpr uint32_t NUM_CASTLES[2] = { 10, 10 };
			constexpr float CASTLE_OFFSET[2] = { 350.0f, 200.0f };

			for (uint32_t i = 0; i < NUM_CASTLES[0]; i++)
			{
				for (uint32_t j = 0; j < NUM_CASTLES[1]; j++)
				{
					Entity e{};
					e.Position = { i * CASTLE_OFFSET[0], 0.0f , j * CASTLE_OFFSET[1] };
					e.Scale = 10.0f * Float3{ 1.0f, 1.0f, 1.0f };
					SceneLoading::AddDraws(scene, e);
				}
			}

			MainSceneGraph.MainCamera.NextTransform.Position += Float3(2 * CASTLE_OFFSET[0], 0.0f, 2 * CASTLE_OFFSET[1]);
		}

	}

	void UpdateInput(float dt, Application* app)
	{
		float dtSec = dt / 1000.0f;

		if (Input::IsKeyJustPressed('R'))
		{
			GFX::Storage::ReloadAllShaders();
			app->OnShaderReload(Device::Get()->GetContext());
			//m_Renderer->OnShaderReload(); TODO
		}

		if (Input::IsKeyJustPressed(VK_TAB))
		{
			if (AppConfig.Settings.contains("HIDE_GUI"))
			{
				Window::Get()->ShowCursor(GUI::Get()->ToggleVisible());
			}
			else
			{
				static bool showCursorToggle = false;
				showCursorToggle = !showCursorToggle;
				Window::Get()->ShowCursor(showCursorToggle);
			}
			
		}

		const float movement_speed = 10.0f;
		const float mouse_speed = 1000.0f;
		char mov_inputs[] = { 'W', 'S', 'A', 'D', 'Q', 'E' };
		Float3 mov_effects[] = { {0.0f, 0.0f, 1.0f},{0.0f, 0.0f, -1.0f},{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},{0.0f, -1.0f, 0.0f},{0.0f, 1.0f, 0.0f} };
		static_assert(STATIC_ARRAY_SIZE(mov_inputs) == STATIC_ARRAY_SIZE(mov_effects));

		float movement_factor = 1.0f;
		if (Input::IsKeyPressed(VK_SHIFT))
		{
			movement_factor = 10.0f;
		}

		Float3 moveDir{ 0.0f, 0.0f, 0.0f };
		for (uint16_t i = 0; i < STATIC_ARRAY_SIZE(mov_inputs); i++)
		{
			if (Input::IsKeyPressed(mov_inputs[i]))
				moveDir += dtSec * movement_factor * movement_speed * mov_effects[i];
		}

		Float4 moveDir4{ moveDir.x, moveDir.y, moveDir.z, 1.0f };
		Float4 relativeDir = Float4(DirectX::XMVector4Transform(moveDir4.ToXM(), MainSceneGraph.MainCamera.WorldToView));
		MainSceneGraph.MainCamera.NextTransform.Position += Float3(relativeDir.x, relativeDir.y, relativeDir.z);

		if (!Window::Get()->IsCursorShown())
		{
			Float2 mouseDelta = Input::GetMouseDelta();
			Float3& cameraRot = MainSceneGraph.MainCamera.NextTransform.Rotation;
			cameraRot.y -= dtSec * mouse_speed * mouseDelta.x;
			cameraRot.x -= dtSec * mouse_speed * mouseDelta.y;
			cameraRot.x = std::clamp(cameraRot.x, -1.5f, 1.5f);
		}

		char camera_inputs[] = { VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT };
		Float2 camera_effects[] = { {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f} };
		static_assert(STATIC_ARRAY_SIZE(camera_inputs) == STATIC_ARRAY_SIZE(camera_effects));

		Float2 cameraDir{ 0.0f, 0.0f };
		for (uint16_t i = 0; i < STATIC_ARRAY_SIZE(camera_inputs); i++)
		{
			if (Input::IsKeyPressed(camera_inputs[i]))
				cameraDir += dtSec * mouse_speed * camera_effects[i];
		}

		Float3& cameraRot = MainSceneGraph.MainCamera.NextTransform.Rotation;
		cameraRot.y += dtSec * mouse_speed * 0.0001f * cameraDir.y;
		cameraRot.x += dtSec * mouse_speed * 0.0001f * cameraDir.x;
		cameraRot.x = std::clamp(cameraRot.x, -1.5f, 1.5f);
	}

}

void ForwardPlus::OnInit(ID3D11DeviceContext* context)
{
	using namespace ForwardPlusPrivate;
	
	// Setup gui
	{
		if (AppConfig.Settings.contains("HIDE_GUI"))
		{
			GUI::Get()->SetVisible(false);
		}

		GUI::Get()->AddElement(new DebugToolsGUI());
		GUI::Get()->AddElement(new PostprocessingGUI());
		GUI::Get()->AddElement(new PositionInfoGUI());
		GUI::Get()->AddElement(new RenderStatsGUI(true));
		GUI::Get()->AddElement(new TextureVisualizerGUI());
		GUI::Get()->AddElement(new LightsGUI());
	}

	// Initialize GFX resources
	{
		SSManager.Init();
		MainSceneGraph.InitRenderData(context);
		VertPipeline.Init(context);

		m_Culling.Init(context);
		m_SkyboxRenderer.Init(context);
		m_DebugRenderer.Init(context);
		m_PostprocessingRenderer.Init(context);
		m_ShadowRenderer.Init(context);
		m_GeometryRenderer.Init(context);

		OnWindowResize(context);
	}
	
	PrepareScene(context);
}

void ForwardPlus::OnDestroy(ID3D11DeviceContext* context)
{

}

TextureID ForwardPlus::OnDraw(ID3D11DeviceContext* context)
{
	using namespace ForwardPlusPrivate;

	// Prepare render targets
	{
		GFX::Cmd::ClearRenderTarget(context, m_MainRT_HDR);
		GFX::Cmd::ClearRenderTarget(context, m_MotionVectorRT);
		GFX::Cmd::ClearDepthStencil(context, m_MainRT_Depth);
		
	}

	MainSceneGraph.FrameUpdate(context);
	CBManager.Bind(context);

	// Geometry culling
	m_Culling.CullGeometries(context);

	// Depth prepass
	const bool drawMotionVectors = PostprocessSettings.AntialiasingMode == AntiAliasingMode::TAA;
	GFX::Cmd::BindRenderTarget(context, drawMotionVectors ? m_MotionVectorRT : TextureID{}, m_MainRT_Depth);
	m_GeometryRenderer.DepthPrepass(context);
	
	// Shadow mask
	TextureID shadowMask = m_ShadowRenderer.CalculateShadowMask(context, m_MainRT_Depth);

	// Light culling
	m_Culling.CullLights(context, m_MainRT_Depth);

	// Geometry
	GFX::Cmd::BindRenderTarget(context, m_MainRT_HDR, m_MainRT_Depth);
	m_GeometryRenderer.Draw(context, shadowMask, m_Culling.GetVisibleLightsBuffer(), m_SkyboxRenderer.GetIrradianceMap());
	
	// Skybox
	GFX::Cmd::BindRenderTarget(context, m_MainRT_HDR, m_MainRT_Depth);
	m_SkyboxRenderer.Draw(context);

	// Debug
	m_DebugRenderer.Draw(context, m_MainRT_HDR, m_MainRT_Depth, m_Culling.GetVisibleLightsBuffer());

	// Postprocessing
	TextureID ppResult = m_PostprocessingRenderer.Process(context, m_MainRT_HDR, m_MainRT_Depth, m_MotionVectorRT);

	return ppResult;
}

void ForwardPlus::OnUpdate(ID3D11DeviceContext* context, float dt)
{
	using namespace ForwardPlusPrivate;
	UpdateInput(dt, this);
}

void ForwardPlus::OnShaderReload(ID3D11DeviceContext* context)
{
	m_SkyboxRenderer.OnShaderReload(context);
}

void ForwardPlus::OnWindowResize(ID3D11DeviceContext* context)
{
	if (m_MainRT_HDR.Valid())
	{
		GFX::Storage::Free(m_MainRT_HDR);
		GFX::Storage::Free(m_MainRT_Depth);
		GFX::Storage::Free(m_MotionVectorRT);
	}

	const uint32_t sampleFlags = PostprocessSettings.AntialiasingMode == AntiAliasingMode::MSAA ? RCF_MSAA_X8 : 0;

	const uint32_t size[2] = { AppConfig.WindowWidth, AppConfig.WindowHeight };
	m_MainRT_HDR = GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV | sampleFlags, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	m_MotionVectorRT = GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV | sampleFlags, 1, DXGI_FORMAT_R16G16_UNORM);
	m_MainRT_Depth = GFX::CreateTexture(size[0], size[1], RCF_Bind_DSV | RCF_Bind_SRV | sampleFlags);
	m_PostprocessingRenderer.ReloadTextureResources(context);
	m_ShadowRenderer.ReloadTextureResources(context);
	m_Culling.UpdateResources(context);

	MainSceneGraph.MainCamera.AspectRatio = (float)AppConfig.WindowWidth / AppConfig.WindowHeight;

}