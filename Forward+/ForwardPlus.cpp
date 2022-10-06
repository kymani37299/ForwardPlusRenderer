#include "ForwardPlus.h"

#include <Engine/Common.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Context.h>
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

#include "Globals.h"
#include "Renderers/Util/ConstantManager.h"
#include "Renderers/Util/SamplerManager.h"
#include "Renderers/Util/VertexPipeline.h"
#include "Scene/SceneLoading.h"
#include "Scene/SceneGraph.h"
#include "Shaders/shared_definitions.h"
#include "Gui/GUI_Implementations.h"

// Globals initialization
DebugVisualizations DebugViz;
RendererSettings RenderSettings;
RenderStatistics RenderStats;

namespace ForwardPlusPrivate
{
	void PrepareScene(GraphicsContext& context)
	{
		MainSceneGraph->AmbientLight = Float3(0.05f, 0.05f, 0.2f);
		MainSceneGraph->DirLight.Direction = Float3(-0.2f, -1.0f, -0.2f);
		MainSceneGraph->DirLight.Radiance = Float3(3.7f, 2.0f, 0.9f);

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

			if (AppConfig.Settings.contains("SINGLE_CASTLE"))
			{
				const Float3 startingPosition{ 350.0f, 0.0f, 200.0f };

				Entity e{};
				e.Position = startingPosition;
				e.Scale = 10.0f * Float3{ 1.0f, 1.0f, 1.0f };
				SceneLoading::AddDraws(scene, e);
				MainSceneGraph->MainCamera.NextTransform.Position += startingPosition;
			}
			else
			{
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
				MainSceneGraph->MainCamera.NextTransform.Position += Float3(2 * CASTLE_OFFSET[0], 0.0f, 2 * CASTLE_OFFSET[1]);
			}

		}
	}

	void UpdateInput(float dt, Application* app)
	{
		float dtSec = dt / 1000.0f;

		if (Input::IsKeyJustPressed('R'))
		{
			GFX::ReloadAllShaders();
			app->OnShaderReload(Device::Get()->GetContext());
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
		Float4 relativeDir = Float4(DirectX::XMVector4Transform(moveDir4.ToXM(), MainSceneGraph->MainCamera.WorldToView));
		MainSceneGraph->MainCamera.NextTransform.Position += Float3(relativeDir.x, relativeDir.y, relativeDir.z);

		if (!Window::Get()->IsCursorShown())
		{
			Float2 mouseDelta = Input::GetMouseDelta();
			Float3& cameraRot = MainSceneGraph->MainCamera.NextTransform.Rotation;
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

		Float3& cameraRot = MainSceneGraph->MainCamera.NextTransform.Rotation;
		cameraRot.y += dtSec * mouse_speed * 0.0001f * cameraDir.y;
		cameraRot.x += dtSec * mouse_speed * 0.0001f * cameraDir.x;
		cameraRot.x = std::clamp(cameraRot.x, -1.5f, 1.5f);
	}

}

ForwardPlus::ForwardPlus()
{

}

ForwardPlus::~ForwardPlus()
{

}

void ForwardPlus::OnInit(GraphicsContext& context)
{
	using namespace ForwardPlusPrivate;
	
	// Setup gui
	{
		if (AppConfig.Settings.contains("HIDE_GUI"))
		{
			GUI::Get()->SetVisible(false);
		}

		GUI::Get()->AddElement(new LightsGUI());
		GUI::Get()->AddElement(new RenderStatsGUI(true));
		GUI::Get()->AddElement(new RenderSettingsGUI());
		GUI::Get()->AddElement(new DebugVisualizationsGUI());
		GUI::Get()->AddElement(new PositionInfoGUI());
	}

	// Initialize GFX resources
	{
		MainSceneGraph = new SceneGraph{};
		MainSceneGraph->InitRenderData(context);

		VertPipeline = new VertexPipeline{};
		VertPipeline->Init(context);

		m_DepthResolveShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/resolve_depth.hlsl"));

		m_Culling.Init(context);
		m_SkyboxRenderer.Init(context);
		m_DebugRenderer.Init(context);
		m_PostprocessingRenderer.Init(context);
		m_ShadowRenderer.Init(context);
		m_GeometryRenderer.Init(context);
		m_SSAORenderer.Init(context);

		OnWindowResize(context);
	}
	
	PrepareScene(context);
}

void ForwardPlus::OnDestroy(GraphicsContext& context)
{
	delete MainSceneGraph;
	delete VertPipeline;
}

Texture* ForwardPlus::OnDraw(GraphicsContext& context)
{
	using namespace ForwardPlusPrivate;

	MainSceneGraph->FrameUpdate(context);

	// Geometry culling
	m_Culling.CullGeometries(context, m_MainRT_Depth.get());

	// Prepare render targets
	{
		GFX::Cmd::ClearRenderTarget(context, m_MainRT_HDR.get());
		GFX::Cmd::ClearRenderTarget(context, m_MotionVectorRT.get());
		GFX::Cmd::ClearDepthStencil(context, m_MainRT_Depth.get());
		GFX::Cmd::ClearDepthStencil(context, m_MainRT_DepthMS.get());
	}

	// Depth prepass
	const bool drawMotionVectors = RenderSettings.AntialiasingMode == AntiAliasingMode::TAA;
	GraphicsState depthPrepassState{};
	if (drawMotionVectors) 	depthPrepassState.RenderTargets.push_back(m_MotionVectorRT.get());
	depthPrepassState.DepthStencil = m_MainRT_DepthMS.get();

	m_GeometryRenderer.DepthPrepass(context, depthPrepassState);
	
	// Resolve depth
	if (RenderSettings.AntialiasingMode == AntiAliasingMode::MSAA)
	{
		GraphicsState resolveState{};
		resolveState.DepthStencilState.DepthEnable = true;
		resolveState.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		CBManager.Clear();
		CBManager.Add(AppConfig.WindowWidth);
		CBManager.Add(AppConfig.WindowHeight);
	
		resolveState.Table.SRVs.push_back(m_MainRT_DepthMS.get());
		resolveState.Table.CBVs.push_back(CBManager.GetBuffer());
	
		GFX::Cmd::MarkerBegin(context, "Resolve depth");
		resolveState.DepthStencil = m_MainRT_Depth.get();
		resolveState.Shader = m_DepthResolveShader.get();
	
		GFX::Cmd::DrawFC(context, resolveState);
		GFX::Cmd::MarkerEnd(context);
	}

	// SSAO
	Texture* ssaoTexture = m_SSAORenderer.Draw(context, m_MainRT_Depth.get());

	// Shadow mask
	Texture* shadowMask = m_ShadowRenderer.CalculateShadowMask(context, m_MainRT_Depth.get());

	// Light culling
	m_Culling.CullLights(context, m_MainRT_Depth.get());

	// Irradiance map
	Texture* irradianceMap = m_SkyboxRenderer.GetIrradianceMap();

	// Geometry
	GraphicsState geometryState;
	geometryState.RenderTargets.push_back(m_MainRT_HDR.get());
	geometryState.DepthStencil = m_MainRT_DepthMS.get();
	m_GeometryRenderer.Draw(context, geometryState, shadowMask, m_Culling.GetVisibleLightsBuffer(), irradianceMap, ssaoTexture);
	
	// Skybox
	GraphicsState skyboxState{};
	skyboxState.RenderTargets.push_back(m_MainRT_HDR.get());
	skyboxState.DepthStencil = m_MainRT_DepthMS.get();
	m_SkyboxRenderer.Draw(context, skyboxState);

	// Debug
	m_DebugRenderer.Draw(context, m_MainRT_HDR.get(), m_MainRT_DepthMS.get(), m_Culling.GetVisibleLightsBuffer());

	// Postprocessing
	Texture* ppResult = m_PostprocessingRenderer.Process(context, m_MainRT_HDR.get(), m_MotionVectorRT.get());
	
	return ppResult;
}

void ForwardPlus::OnUpdate(GraphicsContext& context, float dt)
{
	using namespace ForwardPlusPrivate;
	UpdateInput(dt, this);
}

void ForwardPlus::OnShaderReload(GraphicsContext& context)
{
	m_SkyboxRenderer.OnShaderReload(context);
}

void ForwardPlus::OnWindowResize(GraphicsContext& context)
{
	const uint32_t sampleFlags = RenderSettings.AntialiasingMode == AntiAliasingMode::MSAA ? RCF_MSAA_X8 : 0;

	const uint32_t size[2] = { AppConfig.WindowWidth, AppConfig.WindowHeight };
	m_MainRT_HDR = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | sampleFlags, 1, DXGI_FORMAT_R16G16B16A16_FLOAT));
	m_MotionVectorRT = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | sampleFlags, 1, DXGI_FORMAT_R16G16_UNORM));
	m_MainRT_DepthMS = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF_Bind_DSV | sampleFlags));
	m_MainRT_Depth = RenderSettings.AntialiasingMode == AntiAliasingMode::MSAA  ? Ref<Texture>(GFX::CreateTexture(size[0], size[1], RCF_Bind_DSV)) : Ref<Texture>(m_MainRT_DepthMS);
	m_PostprocessingRenderer.ReloadTextureResources(context);
	m_ShadowRenderer.ReloadTextureResources(context);
	m_Culling.UpdateResources(context);
	m_SSAORenderer.UpdateResources(context);

	MainSceneGraph->MainCamera.AspectRatio = (float)AppConfig.WindowWidth / AppConfig.WindowHeight;

	GFX::SetDebugName(m_MainRT_HDR.get(), "ForwardPlus::MainRT_HDR");
	GFX::SetDebugName(m_MotionVectorRT.get(), "ForwardPlus::MotionVectorRT");
	GFX::SetDebugName(m_MainRT_DepthMS.get(), "ForwardPlus::MainRT_DepthMS");
	GFX::SetDebugName(m_MainRT_Depth.get(), "ForwardPlus::MainRT_Depth");

}