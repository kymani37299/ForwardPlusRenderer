#include "ForwardPlus.h"

#include "Common.h"
#include "Core/SceneGraph.h"
#include "Render/Commands.h"
#include "Render/Texture.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/Device.h"
#include "Render/Resource.h"
#include "Gui/GUI.h"
#include "Gui/GUI_Implementations.h"
#include "System/Input.h"
#include "System/Window.h"
#include "System/ApplicationConfiguration.h"
#include "Loading/SceneLoading.h"
#include "Utility/MathUtility.h"
#include "Utility/Random.h"

namespace ForwardPlusPrivate
{
	void PrepareScene(ID3D11DeviceContext* context)
	{
		MainSceneGraph.CreateAmbientLight(context, Float3(0.1f, 0.1f, 0.15f));
		MainSceneGraph.CreateDirectionalLight(context, Float3(-1.0f, -1.0f, -1.0f), Float3(0.2f, 0.2f, 0.23f));

		constexpr uint32_t NUM_LIGHTS = 10000;
		for (uint32_t i = 0; i < NUM_LIGHTS; i++)
		{
			const float strength = Random::Float(1.0f, 5.0f);
			const Float3 position = Float3(200.0f, 100.0f, 200.0f) * Float3(Random::SNorm(), Random::SNorm(), Random::SNorm());
			const Float3 color = strength * Float3(Random::UNorm(), Random::UNorm(), Random::UNorm());
			const Float2 falloff = strength * Float2(0.5f + 2.0f * Random::UNorm(), 3.0f + 2.0f * Random::UNorm());
			MainSceneGraph.CreatePointLight(context, position, color, falloff);
		}

		if (AppConfig.Settings.contains("SIMPLE_SCENE"))
		{
			Entity plane{};
			plane.Position = { 0.0f, -10.0f, 0.0f };
			plane.Scale = { 10000.0f, 1.0f, 10000.0f };
			uint32_t planeIndex = MainSceneGraph.AddEntity(context, plane);
			SceneLoading::LoadedScene planeScene = SceneLoading::Load("Resources/cube/cube.gltf");
			SceneLoading::AddDraws(planeScene, planeIndex);

			constexpr uint32_t NUM_CUBES = 50;
			for (uint32_t i = 0; i < NUM_CUBES; i++)
			{
				const Float3 position = Float3{ Random::SNorm(), Random::SNorm(), Random::SNorm() } *Float3{ 100.0f, 100.0f, 100.0f };
				const Float3 scale = Float3{ Random::Float(0.1f, 10.0f), Random::Float(0.1f, 10.0f) , Random::Float(0.1f, 10.0f) };
				Entity cube{};
				cube.Position = { 0.0f, -10.0f, 0.0f };
				cube.Scale = { 10000.0f, 1.0f, 10000.0f };
				uint32_t cubeIndex = MainSceneGraph.AddEntity(context, cube);
				SceneLoading::LoadedScene cubeScene = SceneLoading::Load("Resources/cube/cube.gltf");
				SceneLoading::AddDraws(cubeScene, cubeIndex);
			}
		}
		else
		{
			Entity e{};
			e.Position = { 0.0f, 0.0f, 0.0f };
			e.Scale = { 0.1f, 0.1f, 0.1f };
			uint32_t eIndex = MainSceneGraph.AddEntity(context, e);
			SceneLoading::LoadedScene scene = SceneLoading::Load("Resources/sponza/sponza.gltf");
			SceneLoading::AddDraws(scene, eIndex);
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

		Float3 moveDir{ 0.0f, 0.0f, 0.0f };
		for (uint16_t i = 0; i < STATIC_ARRAY_SIZE(mov_inputs); i++)
		{
			if (Input::IsKeyPressed(mov_inputs[i]))
				moveDir += dtSec * movement_speed * mov_effects[i];
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

		GUI::Get()->AddElement(new FPSCounterGUI());
		GUI::Get()->AddElement(new DebugToolsGUI());
		GUI::Get()->AddElement(new PostprocessingGUI());
		GUI::Get()->AddElement(new PositionInfoGUI());
		GUI::Get()->AddElement(new RenderStatsGUI());
	}

	MainSceneGraph.InitRenderData(context);

	m_SkyboxRenderer.Init(context);
	m_DebugRenderer.Init(context);
	m_PostprocessingRenderer.Init(context);
	
	m_DepthPrepassShader = GFX::CreateShader("Source/Shaders/depth.hlsl");
	m_GeometryShader = GFX::CreateShader("Source/Shaders/geometry.hlsl");
	m_LightCullingShader = GFX::CreateShader("Source/Shaders/light_culling.hlsl");

	m_IndexBuffer = GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_IB | RCF_CopyDest);

	UpdatePresentResources(context);
	UpdateCullingResources(context);

	PrepareScene(context);
}

void ForwardPlus::OnDestroy(ID3D11DeviceContext* context)
{

}

bool IsVisible(const Drawable& d)
{
	const Entity& e = MainSceneGraph.Entities[d.EntityIndex];
	const ViewFrustum& vf = MainSceneGraph.MainCamera.CameraFrustum;

	const float maxScale = MAX(MAX(e.Scale.x, e.Scale.y), e.Scale.z);

	BoundingSphere bv;
	bv.Center = e.Position + e.Scale * d.BoundingVolume.Center;
	bv.Radius = d.BoundingVolume.Radius * maxScale;

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

uint32_t PrepareIndexBuffer(ID3D11DeviceContext* context, BufferID indexBuffer, RenderGroup& rg)
{
	std::vector<uint32_t> indices;
	indices.resize(rg.MeshData.GetIndexCount());

	uint32_t indexCount = 0;
	for (uint32_t i = 0; i < rg.Drawables.GetSize(); i++)
	{
		if (rg.VisibilityMask.Get(i))
		{
			const Mesh& mesh = rg.Meshes[rg.Drawables[i].MeshIndex];
			
			uint8_t* copySrc = reinterpret_cast<uint8_t*>(rg.MeshData.GetIndexBuffer().data());
			copySrc += (uint64_t) mesh.IndexOffset * MeshStorage::GetIndexBufferStride();

			uint8_t* copyDst = reinterpret_cast<uint8_t*>(indices.data());
			copyDst += indexCount * sizeof(uint32_t);

			memcpy(copyDst, copySrc, mesh.IndexCount * sizeof(uint32_t));
			indexCount += mesh.IndexCount;
		}
	}

	GFX::ExpandBuffer(context, indexBuffer, indexCount * sizeof(uint32_t));
	GFX::Cmd::UploadToBuffer(context, indexBuffer, 0, indices.data(), 0, indexCount * sizeof(uint32_t));
	return indexCount;
}

TextureID ForwardPlus::OnDraw(ID3D11DeviceContext* context)
{
	using namespace ForwardPlusPrivate;

	MainSceneGraph.FrameUpdate(context);

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
		GFX::Cmd::MarkerBegin(context, "Depth Prepass");
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;

		RenderGroupType prepassTypes[] = { RenderGroupType::Opaque, RenderGroupType::AlphaDiscard };
		for (uint32_t i = 0; i < STATIC_ARRAY_SIZE(prepassTypes); i++)
		{
			RenderGroupType rgType = IntToEnum<RenderGroupType>(i);
			RenderGroup& renderGroup = MainSceneGraph.RenderGroups[i];
			const MeshStorage& meshStorage = renderGroup.MeshData;
			const uint32_t indexCount = PrepareIndexBuffer(context, m_IndexBuffer, renderGroup);

			std::vector<std::string> config{};

			GFX::Cmd::BindRenderTarget(context, m_MotionVectorRT, m_MainRT_Depth);
			GFX::Cmd::SetPipelineState(context, pso);
			GFX::Cmd::BindCBV<VS | PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
			GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.LastFrameCameraBuffer, 1);
			GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.SceneInfoBuffer, 2);
			GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Entities.GetBuffer(), 0);
			GFX::Cmd::BindSRV<VS>(context, renderGroup.Drawables.GetBuffer(), 1);
			if (rgType == RenderGroupType::AlphaDiscard)
			{
				config.push_back("ALPHA_DISCARD");
				GFX::Cmd::SetupStaticSamplers<PS>(context);
				GFX::Cmd::BindSRV<PS>(context, renderGroup.TextureData.GetBuffer(), 3);
				GFX::Cmd::BindSRV<PS>(context, renderGroup.Materials.GetBuffer(), 4);
				GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetDrawableIndexes(), meshStorage.GetTexcoords() });
			}
			else
			{
				GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetDrawableIndexes() });
			}
			GFX::Cmd::BindShader<VS | PS>(context, m_DepthPrepassShader, config, true);
			GFX::Cmd::BindIndexBuffer(context, m_IndexBuffer);
			context->DrawIndexed(indexCount, 0, 0);
		}
		GFX::Cmd::MarkerEnd(context);
	}
	
	// Light culling
	if(!DebugToolsConfig.FreezeLightCulling && !DebugToolsConfig.DisableLightCulling)
	{
		std::vector<std::string> config{};
		if (PostprocessSettings.AntialiasingMode == AntiAliasingMode::MSAA) config.push_back("MULTISAMPLE_DEPTH");

		GFX::Cmd::BindRenderTarget(context, TextureID{}, TextureID{});
		GFX::Cmd::MarkerBegin(context, "Light Culling");
		GFX::Cmd::BindShader<CS>(context, m_LightCullingShader, config);
		GFX::Cmd::BindCBV<CS>(context, MainSceneGraph.SceneInfoBuffer, 0);
		GFX::Cmd::BindCBV<CS>(context, MainSceneGraph.MainCamera.CameraBuffer, 2);
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
		
		// Initial setup
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
		GFX::Cmd::BindCBV<VS|PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.SceneInfoBuffer, 1);
		GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Lights.GetBuffer(), 3);
		GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Entities.GetBuffer(), 5);
		GFX::Cmd::BindSRV<PS>(context, m_VisibleLightsBuffer, 8);
		GFX::Cmd::BindIndexBuffer(context, m_IndexBuffer);

		for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
		{
			RenderGroupType rgType = (RenderGroupType) i;
			RenderGroup& renderGroup = MainSceneGraph.RenderGroups[i];
			if (renderGroup.Drawables.GetSize() == 0) continue;

			const MeshStorage& meshStorage = renderGroup.MeshData;
			const uint32_t indexCount = PrepareIndexBuffer(context, m_IndexBuffer, renderGroup);

			std::vector<std::string> configuration;
			if (rgType == RenderGroupType::AlphaDiscard) configuration.push_back("ALPHA_DISCARD");
			if (DebugToolsConfig.DisableLightCulling) configuration.push_back("DISABLE_LIGHT_CULLING");
			if (DebugToolsConfig.UsePBR) configuration.push_back("USE_PBR");

			GFX::Cmd::BindShader<VS|PS>(context, m_GeometryShader, configuration, true);
			GFX::Cmd::BindSRV<PS>(context, renderGroup.TextureData.GetBuffer(), 0);
			GFX::Cmd::BindSRV<PS>(context, renderGroup.Materials.GetBuffer(), 6);
			GFX::Cmd::BindSRV<VS|PS>(context, renderGroup.Drawables.GetBuffer(), 7);
			GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetTexcoords(), meshStorage.GetNormals(), meshStorage.GetTangents(), meshStorage.GetDrawableIndexes() });
			context->DrawIndexed(indexCount, 0, 0);
		}

		GFX::Cmd::MarkerEnd(context);
	}
	
	// Skybox
	GFX::Cmd::BindRenderTarget(context, m_MainRT_HDR, m_MainRT_Depth);
	m_SkyboxRenderer.Draw(context);

	// Debug
	m_DebugRenderer.Draw(context, m_MainRT_HDR, m_MainRT_Depth, m_VisibleLightsBuffer);

	// Postprocessing
	TextureID ppResult = m_PostprocessingRenderer.Process(context, m_MainRT_HDR, m_MotionVectorRT);

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
}

void ForwardPlus::UpdateCullingResources(ID3D11DeviceContext* context)
{
	if(m_VisibleLightsBuffer.Valid())
		GFX::Storage::Free(m_VisibleLightsBuffer);

	m_NumTilesX = MathUtility::CeilDiv(AppConfig.WindowWidth, TILE_SIZE);
	m_NumTilesY = MathUtility::CeilDiv(AppConfig.WindowHeight, TILE_SIZE);
	m_VisibleLightsBuffer = GFX::CreateBuffer(m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_SB | RCF_Bind_UAV);
}