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
			Float3 position = Float3(200.0f, 100.0f, 200.0f) * Float3(Random::SNorm(), Random::SNorm(), Random::SNorm());
			Float3 color = Float3(Random::UNorm(), Random::UNorm(), Random::UNorm());
			Float2 falloff = Float2(1.0f + 3.0f * Random::UNorm(), 5.0f + 10.0f * Random::UNorm());
			MainSceneGraph.CreatePointLight(context, position, color, falloff);
		}

		if (AppConfig.Settings.contains("SIMPLE_SCENE"))
		{
			Entity& plane = MainSceneGraph.CreateEntity(context, { 0.0f, -10.0f, 0.0f }, { 10000.0f, 1.0f, 10000.0f });
			SceneLoading::LoadEntity("Resources/cube/cube.gltf", plane);

			constexpr uint32_t NUM_CUBES = 50;
			for (uint32_t i = 0; i < NUM_CUBES; i++)
			{
				const Float3 position = Float3{ Random::SNorm(), Random::SNorm(), Random::SNorm() } *Float3{ 100.0f, 100.0f, 100.0f };
				const Float3 scale = Float3{ Random::Float(0.1f, 10.0f), Random::Float(0.1f, 10.0f) , Random::Float(0.1f, 10.0f) };
				Entity& cube = MainSceneGraph.CreateEntity(context, position, scale);
				SceneLoading::LoadEntity("Resources/cube/cube.gltf", cube);
			}
		}
		else
		{
			Entity& e1 = MainSceneGraph.CreateEntity(context, { 0.0f, 0.0f, 0.0f }, { 0.1f, 0.1f, 0.1f });
			Entity& e2 = MainSceneGraph.CreateEntity(context);

			SceneLoading::LoadEntityInBackground("Resources/sponza/sponza.gltf", e1);
			SceneLoading::LoadEntity("Resources/cube/cube.gltf", e2);
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
	
	m_DepthPrepassShader = GFX::CreateShader("Source/Shaders/depth.hlsl");
	m_GeometryShader = GFX::CreateShader("Source/Shaders/geometry.hlsl");
	m_LightCullingShader = GFX::CreateShader("Source/Shaders/light_culling.hlsl");
	m_PostprocessShader = GFX::CreateShader("Source/Shaders/postprocessing.hlsl");

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

	MainSceneGraph.MainCamera.UseJitter = PostprocessSettings.EnableTAA;
	MainSceneGraph.FrameUpdate(context);
	UpdatePostprocessingSettings(context);

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
		GFX::Cmd::ClearRenderTarget(context, m_MainRT_LDR);
		GFX::Cmd::ClearRenderTarget(context, m_MotionVectorRT);
		GFX::Cmd::ClearDepthStencil(context, m_MainRT_Depth);
	}

	// Depth prepass
	{
		GFX::Cmd::MarkerBegin(context, "Depth Prepass");
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;

		RenderGroup& renderGroup = MainSceneGraph.RenderGroups[EnumToInt(RenderGroupType::Opaque)];
		const MeshStorage& meshStorage = renderGroup.MeshData;
		const uint32_t indexCount = PrepareIndexBuffer(context, m_IndexBuffer, renderGroup);

		GFX::Cmd::BindRenderTarget(context, m_MotionVectorRT, m_MainRT_Depth);
		GFX::Cmd::SetPipelineState(context, pso);
		GFX::Cmd::BindShader<VS|PS>(context, m_DepthPrepassShader, {}, true);
		GFX::Cmd::BindCBV<VS|PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.LastFrameCameraBuffer, 1);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.SceneInfoBuffer, 2);
		GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Entities.GetBuffer(), 0);
		GFX::Cmd::BindSRV<VS>(context, renderGroup.Drawables.GetBuffer(), 1);
		GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetDrawableIndexes() });
		GFX::Cmd::BindIndexBuffer(context, m_IndexBuffer);
		context->DrawIndexed(indexCount, 0, 0);
		GFX::Cmd::MarkerEnd(context);
	}
	
	// Light culling
	if(!DebugToolsConfig.FreezeLightCulling && !DebugToolsConfig.DisableLightCulling)
	{
		GFX::Cmd::BindRenderTarget(context, TextureID{}, TextureID{});
		GFX::Cmd::MarkerBegin(context, "Light Culling");
		GFX::Cmd::BindShader<CS>(context, m_LightCullingShader);
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

			GFX::Cmd::BindShader<VS|PS>(context, m_GeometryShader, configuration, true);
			GFX::Cmd::BindSRV<PS>(context, renderGroup.TextureData, 0);
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

	// Postprocessing
	{
		GFX::Cmd::MarkerBegin(context, "Postprocessing");

		// Tonemapping
		{
			std::vector<std::string> config{};
			config.push_back("TONEMAPPING");
			if (PostprocessSettings.UseExposureTonemapping) config.push_back("EXPOSURE");
			else config.push_back("REINHARD");

			GFX::Cmd::MarkerBegin(context, "Tonemapping");
			GFX::Cmd::SetupStaticSamplers<PS>(context);
			GFX::Cmd::BindRenderTarget(context, m_MainRT_LDR);
			GFX::Cmd::BindShader<PS | VS>(context, m_PostprocessShader, config);
			GFX::Cmd::BindCBV<PS>(context, m_PostprocessingSettingsBuffer, 0);
			GFX::Cmd::BindSRV<PS>(context, m_MainRT_HDR, 0);
			GFX::Cmd::BindVertexBuffer(context, Device::Get()->GetQuadBuffer());
			context->Draw(6, 0);
			GFX::Cmd::MarkerEnd(context);
		}

		// TAA
		if (PostprocessSettings.EnableTAA)
		{
			// TAA History
			GFX::Cmd::CopyToTexture(context, m_TAAHistory[0], m_TAAHistory[1]);
			GFX::Cmd::CopyToTexture(context, m_MainRT_LDR, m_TAAHistory[0]);

			GFX::Cmd::MarkerBegin(context, "TAA");
			GFX::Cmd::BindRenderTarget(context, m_MainRT_LDR);
			GFX::Cmd::BindShader<VS | PS>(context, m_PostprocessShader, { "TAA" });
			GFX::Cmd::SetupStaticSamplers<PS>(context);
			GFX::Cmd::BindSRV<PS>(context, m_TAAHistory[0], 0);
			GFX::Cmd::BindSRV<PS>(context, m_TAAHistory[1], 1);
			GFX::Cmd::BindSRV<PS>(context, m_MotionVectorRT, 2);
			GFX::Cmd::BindVertexBuffer(context, Device::Get()->GetQuadBuffer());
			context->Draw(6, 0);
			GFX::Cmd::MarkerEnd(context);
		}

		GFX::Cmd::MarkerEnd(context);
	}

	m_DebugRenderer.Draw(context, m_MainRT_LDR, m_MainRT_Depth, m_VisibleLightsBuffer);

	return m_MainRT_LDR;
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
		GFX::Storage::Free(m_MainRT_LDR);
		GFX::Storage::Free(m_MainRT_Depth);
		GFX::Storage::Free(m_MotionVectorRT);
		GFX::Storage::Free(m_TAAHistory[0]);
		GFX::Storage::Free(m_TAAHistory[1]);
	}

	const uint32_t size[2] = { AppConfig.WindowWidth, AppConfig.WindowHeight };
	m_MainRT_HDR		= GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	m_MainRT_LDR		= GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV);
	m_MotionVectorRT	= GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R16G16_UNORM);
	m_MainRT_Depth		= GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_DSV | RCF_Bind_SRV);
	m_TAAHistory[0]		= GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_SRV | RCF_CopyDest);
	m_TAAHistory[1]		= GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_SRV | RCF_CopyDest);

	// Halton sequence
	const Float2 haltonSequence[16] = { {0.500000,0.333333},
							{0.250000,0.666667},
							{0.750000,0.111111},
							{0.125000,0.444444},
							{0.625000,0.777778},
							{0.375000,0.222222},
							{0.875000,0.555556},
							{0.062500,0.888889},
							{0.562500,0.037037},
							{0.312500,0.370370},
							{0.812500,0.703704},
							{0.187500,0.148148},
							{0.687500,0.481481},
							{0.437500,0.814815},
							{0.937500,0.259259},
							{0.031250,0.592593} };

	MainSceneGraph.MainCamera.UseJitter = true;
	for (uint32_t i = 0; i < 16; i++)
	{
		MainSceneGraph.MainCamera.Jitter[i] = 2.0f * ((haltonSequence[i] - Float2{ 0.5f, 0.5f }) / Float2(AppConfig.WindowWidth, AppConfig.WindowHeight));
	}
}

void ForwardPlus::UpdateCullingResources(ID3D11DeviceContext* context)
{
	if(m_VisibleLightsBuffer.Valid())
		GFX::Storage::Free(m_VisibleLightsBuffer);

	m_NumTilesX = MathUtility::CeilDiv(AppConfig.WindowWidth, TILE_SIZE);
	m_NumTilesY = MathUtility::CeilDiv(AppConfig.WindowHeight, TILE_SIZE);
	m_VisibleLightsBuffer = GFX::CreateBuffer(m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_SB | RCF_Bind_UAV);
}

void ForwardPlus::UpdatePostprocessingSettings(ID3D11DeviceContext* context)
{
	struct PostprocessingSettngsCB
	{
		float Exposure;
	};

	if(!m_PostprocessingSettingsBuffer.Valid())
		m_PostprocessingSettingsBuffer = GFX::CreateConstantBuffer<PostprocessingSettngsCB>();

	PostprocessingSettngsCB cb{};
	cb.Exposure = PostprocessSettings.Exposure;

	GFX::Cmd::UploadToBuffer(context, m_PostprocessingSettingsBuffer, 0, &cb, 0, sizeof(PostprocessingSettngsCB));
}