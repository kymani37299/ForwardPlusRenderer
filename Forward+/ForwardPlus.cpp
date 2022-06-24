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
		MainSceneGraph.InitRenderData(context);
		VertPipeline.Init(context);

		m_SkyboxRenderer.Init(context);
		m_DebugRenderer.Init(context);
		m_PostprocessingRenderer.Init(context);
		m_ShadowRenderer.Init(context);

		m_DepthPrepassShader = GFX::CreateShader("Forward+/Shaders/depth.hlsl");
		m_GeometryShader = GFX::CreateShader("Forward+/Shaders/geometry.hlsl");
		m_LightCullingShader = GFX::CreateShader("Forward+/Shaders/light_culling.hlsl");

		UpdatePresentResources(context);
		UpdateCullingResources(context);
	}
	
	PrepareScene(context);
}

void ForwardPlus::OnDestroy(ID3D11DeviceContext* context)
{

}

bool IsVisible(const Drawable& d)
{
	const Entity& e = MainSceneGraph.Entities[d.EntityIndex];
	const ViewFrustum& vf = MainSceneGraph.MainCamera.CameraFrustum;
	const BoundingSphere bv = e.GetBoundingVolume(d.BoundingVolume);

	return vf.IsInFrustum(bv);
}

void CullRenderGroup(RenderGroup& rg)
{
	const uint32_t numDrawables = rg.Drawables.GetSize();
	rg.VisibilityMask = BitField{ numDrawables };
	for (uint32_t i = 0; i < numDrawables; i++)
	{
		Drawable& d = rg.Drawables[i];
		if (d.DrawableIndex != Drawable::InvalidIndex && IsVisible(d))
		{
			rg.VisibilityMask.Set(i, true);
		}
	}
}

TextureID ForwardPlus::OnDraw(ID3D11DeviceContext* context)
{
	using namespace ForwardPlusPrivate;

	MainSceneGraph.FrameUpdate(context);

	CBManager.Bind(context);

	// Geometry culling
	if (!DebugToolsConfig.FreezeGeometryCulling)
	{
		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			RenderGroup& rg = MainSceneGraph.RenderGroups[i];
			if (DebugToolsConfig.DisableGeometryCulling)
			{
				uint32_t numDrawables = rg.Drawables.GetSize();
				rg.VisibilityMask = BitField(numDrawables);
				for (uint32_t i = 0; i < numDrawables; i++) rg.VisibilityMask.Set(i, true);
			}
			else
			{
				CullRenderGroup(rg);
			}
		}
	}

	// Prepare render targets
	{
		GFX::Cmd::ClearRenderTarget(context, m_MainRT_HDR);
		GFX::Cmd::ClearRenderTarget(context, m_MotionVectorRT);
		GFX::Cmd::ClearDepthStencil(context, m_MainRT_Depth);
		
	}

	// Depth prepass
	{
		const bool drawMotionVectors = PostprocessSettings.AntialiasingMode == AntiAliasingMode::TAA;
		
		GFX::Cmd::MarkerBegin(context, "Depth Prepass");
		
		CBManager.Clear();
		CBManager.Add(MainSceneGraph.MainCamera.CameraData);
		CBManager.Add(MainSceneGraph.MainCamera.LastCameraData);
		CBManager.Add(MainSceneGraph.SceneInfoData, true);
		
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;
	
		RenderGroupType prepassTypes[] = { RenderGroupType::Opaque, RenderGroupType::AlphaDiscard };
		for (uint32_t i = 0; i < STATIC_ARRAY_SIZE(prepassTypes); i++)
		{
			RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
			RenderGroup& renderGroup = MainSceneGraph.RenderGroups[i];
	
			std::vector<std::string> config{};
			if (drawMotionVectors) config.push_back("MOTION_VECTORS");
	
			GFX::Cmd::BindRenderTarget(context, drawMotionVectors ? m_MotionVectorRT : TextureID{}, m_MainRT_Depth);
			GFX::Cmd::SetPipelineState(context, pso);

			if (rgType == RenderGroupType::AlphaDiscard)
			{
				config.push_back("ALPHA_DISCARD");
				GFX::Cmd::SetupStaticSamplers<PS>(context);
			}
			GFX::Cmd::BindShader<VS | PS>(context, m_DepthPrepassShader, config, true);
			VertPipeline.Draw(context, renderGroup);
		}
		GFX::Cmd::MarkerEnd(context);
	}
	
	TextureID shadowMask = m_ShadowRenderer.CalculateShadowMask(context, m_MainRT_Depth);

	// Light culling
	if(!DebugToolsConfig.FreezeLightCulling && !DebugToolsConfig.DisableLightCulling)
	{
		std::vector<std::string> config{};
		if (PostprocessSettings.AntialiasingMode == AntiAliasingMode::MSAA) config.push_back("MULTISAMPLE_DEPTH");

		CBManager.Clear();
		CBManager.Add(MainSceneGraph.SceneInfoData);
		CBManager.Add(MainSceneGraph.MainCamera.CameraData, true);

		GFX::Cmd::BindRenderTarget(context, TextureID{}, TextureID{});
		GFX::Cmd::MarkerBegin(context, "Light Culling");
		GFX::Cmd::BindShader<CS>(context, m_LightCullingShader, config);
		GFX::Cmd::BindSRV<CS>(context, MainSceneGraph.Lights.GetBuffer(), 0);
		GFX::Cmd::BindSRV<CS>(context, m_MainRT_Depth, 1);
		GFX::Cmd::BindUAV<CS>(context, m_VisibleLightsBuffer, 0);
		context->Dispatch(m_NumTilesX, m_NumTilesY, 1);
		GFX::Cmd::BindUAV<CS>(context, nullptr, 0);
		GFX::Cmd::BindSRV<CS>(context, nullptr, 1);
		GFX::Cmd::MarkerEnd(context);
	}

	// Geometry
	{
		GFX::Cmd::MarkerBegin(context, "Geometry");
		
		CBManager.Clear();
		CBManager.Add(MainSceneGraph.MainCamera.CameraData);
		CBManager.Add(MainSceneGraph.SceneInfoData, true);

		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;
		pso.DS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		pso.DS.StencilEnable = true;
		pso.DS.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		pso.DS.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		pso.DS.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		pso.DS.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		pso.DS.BackFace = pso.DS.FrontFace;
		GFX::Cmd::SetPipelineState(context, pso);

		GFX::Cmd::BindRenderTarget(context, m_MainRT_HDR, m_MainRT_Depth);
		GFX::Cmd::SetupStaticSamplers<PS>(context);

		GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Lights.GetBuffer(), 0);
		GFX::Cmd::BindSRV<PS>(context, m_VisibleLightsBuffer, 1);
		GFX::Cmd::BindSRV<PS>(context, shadowMask, 2);
		GFX::Cmd::BindSRV<PS>(context, m_SkyboxRenderer.GetIrradianceMap(), 3);

		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			RenderGroupType rgType = (RenderGroupType) i;
			RenderGroup& renderGroup = MainSceneGraph.RenderGroups[i];
			if (renderGroup.Drawables.GetSize() == 0) continue;
			
			std::vector<std::string> configuration;
			if (rgType == RenderGroupType::AlphaDiscard) configuration.push_back("ALPHA_DISCARD");
			if (DebugToolsConfig.DisableLightCulling) configuration.push_back("DISABLE_LIGHT_CULLING");
			if (DebugToolsConfig.UsePBR) configuration.push_back("USE_PBR");
			if (DebugToolsConfig.UsePBR && DebugToolsConfig.UseIBL) configuration.push_back("USE_IBL");

			GFX::Cmd::BindShader<VS|PS>(context, m_GeometryShader, configuration, true);
			VertPipeline.Draw(context, renderGroup);
		}

		GFX::Cmd::MarkerEnd(context);
	}
	
	// Skybox
	GFX::Cmd::BindRenderTarget(context, m_MainRT_HDR, m_MainRT_Depth);
	m_SkyboxRenderer.Draw(context);

	// Debug
	m_DebugRenderer.Draw(context, m_MainRT_HDR, m_MainRT_Depth, m_VisibleLightsBuffer);

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
	UpdatePresentResources(context);
	UpdateCullingResources(context);
}

void ForwardPlus::UpdatePresentResources(ID3D11DeviceContext* context)
{
	if (m_MainRT_HDR.Valid())
	{
		GFX::Storage::Free(m_MainRT_HDR);
		GFX::Storage::Free(m_MainRT_Depth);
		GFX::Storage::Free(m_MotionVectorRT);
	}

	const uint32_t sampleFlags = PostprocessSettings.AntialiasingMode == AntiAliasingMode::MSAA ? RCF_MSAA_X8 : 0;
	
	const uint32_t size[2] = { AppConfig.WindowWidth, AppConfig.WindowHeight };
	m_MainRT_HDR		= GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV | sampleFlags, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	m_MotionVectorRT	= GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV | sampleFlags, 1, DXGI_FORMAT_R16G16_UNORM);
	m_MainRT_Depth		= GFX::CreateTexture(size[0], size[1], RCF_Bind_DSV | RCF_Bind_SRV | sampleFlags);
	m_PostprocessingRenderer.ReloadTextureResources(context);
	m_ShadowRenderer.ReloadTextureResources(context);

	MainSceneGraph.MainCamera.AspectRatio = (float)AppConfig.WindowWidth / AppConfig.WindowHeight;
}

void ForwardPlus::UpdateCullingResources(ID3D11DeviceContext* context)
{
	if(m_VisibleLightsBuffer.Valid())
		GFX::Storage::Free(m_VisibleLightsBuffer);

	m_NumTilesX = MathUtility::CeilDiv(AppConfig.WindowWidth, TILE_SIZE);
	m_NumTilesY = MathUtility::CeilDiv(AppConfig.WindowHeight, TILE_SIZE);
	m_VisibleLightsBuffer = GFX::CreateBuffer(m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_SB | RCF_Bind_UAV);
}