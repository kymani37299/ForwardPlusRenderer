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

#include "Globals.h"
#include "Renderers/Util/ConstantBuffer.h"
#include "Renderers/Util/VertexPipeline.h"
#include "Renderers/Util/TextureDebugger.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneGraph.h"
#include "Shaders/shared_definitions.h"
#include "Gui/GUI_Implementations.h"

// Globals initialization
DebugVisualizations DebugViz;
RendererSettings RenderSettings;
RenderStatistics RenderStats;

// Until initialization
TextureDebugger TexDebugger;

namespace ForwardPlusPrivate
{
	void UpdateInput(GraphicsContext& context, float dt, Application* app)
	{
		PROFILE_SECTION_CPU("UpdateInput");

		float dtSec = dt / 1000.0f;

		if (Input::IsKeyJustPressed('R'))
		{
			GFX::ReloadAllShaders();
			app->OnShaderReload(context);
		}

		if (Input::IsKeyJustPressed('F'))
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
		Float4 relativeDir = Float4(DirectX::XMVector4Transform(moveDir4.ToXM(), SceneManager::Get().GetSceneGraph().MainCamera.WorldToView));
		SceneManager::Get().GetSceneGraph().MainCamera.NextTransform.Position += Float3(relativeDir.x, relativeDir.y, relativeDir.z);

		if (!Window::Get()->IsCursorShown())
		{
			Float2 mouseDelta = Input::GetMouseDelta();
			Float3& cameraRot = SceneManager::Get().GetSceneGraph().MainCamera.NextTransform.Rotation;
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

		Float3& cameraRot = SceneManager::Get().GetSceneGraph().MainCamera.NextTransform.Rotation;
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
	const bool profileLoading = AppConfig.Settings.contains("PROFILE_LOADING");
	if (profileLoading)
	{
		OPTICK_START_CAPTURE();
	}
	PROFILE_SECTION(context, "ForwardPlus initialization");

	using namespace ForwardPlusPrivate;
	
	// Setup gui
	{
		if (AppConfig.Settings.contains("HIDE_GUI"))
		{
			GUI::Get()->SetVisible(false);
		}

		GUI::Get()->AddElement(new SceneManagerGUI());
		GUI::Get()->AddElement(new LightsGUI());
		GUI::Get()->AddElement(new RenderStatsGUI());
		GUI::Get()->AddElement(new RenderSettingsGUI());
		GUI::Get()->AddElement(new DebugVisualizationsGUI());
		GUI::Get()->AddElement(new PositionInfoGUI());
		GUI::Get()->AddElement(new TextureDebuggerGUI());
	}

	// Load scene
	{
#ifdef DEBUG
		const SceneSelection scene = SceneSelection::SimpleBoxes;
#else
		const SceneSelection scene = SceneSelection::Sponza;
#endif
		SceneManager::Get().LoadScene(context, scene);
	}

	// Initialize GFX resources
	{
		PROFILE_SECTION(context, "Initialize GFX Resources");

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
		m_TextureDebuggerRenderer.Init(context);

		OnWindowResize(context);
	}

	if (profileLoading)
	{
		OPTICK_STOP_CAPTURE();
		OPTICK_SAVE_CAPTURE("LoadingCapture.opt");
	}
}

void ForwardPlus::OnDestroy(GraphicsContext& context)
{
	SceneManager::Destroy();
	delete VertPipeline;
}

Texture* ForwardPlus::OnDraw(GraphicsContext& context)
{
	using namespace ForwardPlusPrivate;

	SceneManager::Get().GetSceneGraph().FrameUpdate(context);

	// Geometry culling
	if (!RenderSettings.Culling.GeometryCullingFrozen)
	{
		PROFILE_SECTION(context, "Geometry Culling");
		
		const bool useHzb = RenderSettings.Culling.GeoCullingMode == GeometryCullingMode::GPU_OcclusionCulling;

		// Main camera
		GeometryCullingInput mainCameraInput{ SceneManager::Get().GetSceneGraph().MainCamera };
		if(useHzb) mainCameraInput.HZB = m_GeometryRenderer.GetHZB(context, m_MainRT_Depth.get());
		m_Culling.CullGeometries(context, mainCameraInput);

		// Shadow camera
		GeometryCullingInput shadowCameraInput{ SceneManager::Get().GetSceneGraph().ShadowCamera };
		if (useHzb) shadowCameraInput.HZB = m_ShadowRenderer.GetHZB(context);
		m_Culling.CullGeometries(context, shadowCameraInput);
	}
	

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
	if (drawMotionVectors) 	depthPrepassState.RenderTargets[0] = m_MotionVectorRT.get();
	depthPrepassState.DepthStencil = m_MainRT_DepthMS.get();

	m_GeometryRenderer.DepthPrepass(context, depthPrepassState);
	
	// Resolve depth
	if (RenderSettings.AntialiasingMode == AntiAliasingMode::MSAA)
	{
		PROFILE_SECTION(context, "Resolve depth");

		GraphicsState resolveState{};
		resolveState.DepthStencilState.DepthEnable = true;
		resolveState.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		ConstantBuffer cb{};
		cb.Add(AppConfig.WindowWidth);
		cb.Add(AppConfig.WindowHeight);
	
		resolveState.Table.SRVs[0] = m_MainRT_DepthMS.get();
		resolveState.Table.CBVs[0] = cb.GetBuffer(context);
		resolveState.DepthStencil = m_MainRT_Depth.get();
		resolveState.Shader = m_DepthResolveShader.get();
	
		GFX::Cmd::DrawFC(context, resolveState);
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
	geometryState.RenderTargets[0] = m_MainRT_HDR.get();
	geometryState.DepthStencil = m_MainRT_DepthMS.get();
	m_GeometryRenderer.Draw(context, geometryState, shadowMask, m_Culling.GetVisibleLightsBuffer(), irradianceMap, ssaoTexture);
	
	// Skybox
	GraphicsState skyboxState{};
	skyboxState.RenderTargets[0] = m_MainRT_HDR.get();
	skyboxState.DepthStencil = m_MainRT_DepthMS.get();
	m_SkyboxRenderer.Draw(context, skyboxState);

	// Debug
	m_DebugRenderer.Draw(context, m_MainRT_HDR.get(), m_MainRT_DepthMS.get(), m_Culling.GetVisibleLightsBuffer());
	m_TextureDebuggerRenderer.Draw(context);

	// Postprocessing
	Texture* ppResult = m_PostprocessingRenderer.Process(context, m_MainRT_HDR.get(), m_MotionVectorRT.get());
	
	// Update RenderStats
	RenderStats.MainStats = SceneManager::Get().GetSceneGraph().MainCamera.CullingData.CullingStats;
	RenderStats.ShadowStats = SceneManager::Get().GetSceneGraph().ShadowCamera.CullingData.CullingStats;

	return ppResult;
}

void ForwardPlus::OnUpdate(GraphicsContext& context, float dt)
{
	using namespace ForwardPlusPrivate;
	UpdateInput(context, dt, this);
}

void ForwardPlus::OnShaderReload(GraphicsContext& context)
{
	m_SkyboxRenderer.OnShaderReload(context);
}

void ForwardPlus::OnWindowResize(GraphicsContext& context)
{
	const RCF sampleFlags = RenderSettings.AntialiasingMode == AntiAliasingMode::MSAA ? RCF::MSAA_X8 : RCF::None;

	const uint32_t size[2] = { AppConfig.WindowWidth, AppConfig.WindowHeight };
	m_MainRT_HDR = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF::RTV | sampleFlags, 1, DXGI_FORMAT_R16G16B16A16_FLOAT));
	m_MotionVectorRT = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF::RTV | sampleFlags, 1, DXGI_FORMAT_R16G16_UNORM));
	m_MainRT_DepthMS = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF::DSV | sampleFlags));
	m_MainRT_Depth = RenderSettings.AntialiasingMode == AntiAliasingMode::MSAA  ? Ref<Texture>(GFX::CreateTexture(size[0], size[1], RCF::DSV)) : Ref<Texture>(m_MainRT_DepthMS);
	m_PostprocessingRenderer.ReloadTextureResources(context);
	m_ShadowRenderer.ReloadTextureResources(context);
	m_Culling.UpdateResources(context);
	m_SSAORenderer.UpdateResources(context);

	SceneManager::Get().GetSceneGraph().MainCamera.AspectRatio = (float)AppConfig.WindowWidth / AppConfig.WindowHeight;

	GFX::SetDebugName(m_MainRT_HDR.get(), "ForwardPlus::MainRT_HDR");
	GFX::SetDebugName(m_MotionVectorRT.get(), "ForwardPlus::MotionVectorRT");
	GFX::SetDebugName(m_MainRT_DepthMS.get(), "ForwardPlus::MainRT_DepthMS");
	GFX::SetDebugName(m_MainRT_Depth.get(), "ForwardPlus::MainRT_Depth");

	TexDebugger.AddTexture("MainRT_Depth", m_MainRT_Depth.get());

	// Hacky but works !
	TexDebugger.AddTexture("Main HZB", m_GeometryRenderer.GetHZB(context, m_MainRT_Depth.get()));
	TexDebugger.AddTexture("Shadow HZB", m_ShadowRenderer.GetHZB(context));
}