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
#include "Gui/FPSCounterGUI.h"
#include "Gui/DebugToolsGUI.h"
#include "Gui/PositionInfoGUI.h"
#include "Gui/RenderStatsGUI.h"
#include "System/Input.h"
#include "System/Window.h"
#include "System/ApplicationConfiguration.h"
#include "Loading/SceneLoading.h"
#include "Utility/MathUtility.h"
#include "Utility/Random.h"

namespace ForwardPlusPrivate
{
	BufferID CubeVB;

	TextureID PanoramaToCubemap(ID3D11DeviceContext* context, TextureID panoramaTexture, uint32_t cubemapSize)
	{
		const auto getCameraForFace = [](uint32_t faceIndex)
		{
			using namespace DirectX;
			static Float3 forwards[6] = { Float3(-1.0f,  0.0f,  0.0f), Float3(1.0f,  0.0f,  0.0f), Float3(0.0f,  -1.0f,  0.0f), Float3(0.0f, 1.0f,  0.0f), Float3(0.0f,  0.0f,  1.0f), Float3(0.0f,  0.0f, -1.0f) };
			static Float3 ups[6] = { Float3(0.0f, -1.0f,  0.0f), Float3(0.0f, -1.0f,  0.0f), Float3(0.0f,  0.0f,  -1.0f), Float3(0.0f,  0.0f, 1.0f), Float3(0.0f, -1.0f,  0.0f), Float3(0.0f, -1.0f,  0.0f) };

			XMMATRIX viewMat = XMMatrixLookAtLH(Float3(0.0f, 0.0f, 0.0f).ToXM(), forwards[faceIndex].ToXM(), ups[faceIndex].ToXM());
			XMMATRIX projMat = XMMatrixOrthographicLH(2.0f, 2.0f, 0.1f, 10.0f);
			XMMATRIX viewProj = XMMatrixMultiply(viewMat, projMat);
			viewProj = XMMatrixTranspose(viewProj);
			XMFLOAT4X4 ret;
			XMStoreFloat4x4(&ret, viewProj);
			return ret;
		};

		TextureID cubemapTex = GFX::CreateTextureArray(cubemapSize, cubemapSize, 6, RCF_Bind_SRV | RCF_Bind_RTV | RCF_Cubemap);
		const Texture& cubemapTexHandle = GFX::Storage::GetTexture(cubemapTex);
		BufferID cameraCB = GFX::CreateConstantBuffer<DirectX::XMFLOAT4X4>();
		ShaderID shader = GFX::CreateShader("Source/Shaders/quadrilateral2cubemap.hlsl");

		GFX::Cmd::MarkerBegin(context, "PanoramaToCubemap");

		GFX::Cmd::BindVertexBuffer(context, CubeVB);
		GFX::Cmd::BindShader(context, shader);
		GFX::Cmd::BindCBV<VS>(context, cameraCB, 0);
		GFX::Cmd::BindSRV<PS>(context, panoramaTexture, 0);
		GFX::Cmd::SetupStaticSamplers<PS>(context);
		GFX::Cmd::SetViewport(context, { (float)cubemapSize, (float)cubemapSize });

		for (uint32_t i = 0; i < 6; i++)
		{
			// Create RTV
			ID3D11RenderTargetView* rtv;
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = cubemapTexHandle.Format;
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.FirstArraySlice = i;
			rtvDesc.Texture2DArray.MipSlice = 0;
			API_CALL(Device::Get()->GetHandle()->CreateRenderTargetView(cubemapTexHandle.Handle.Get(), &rtvDesc, &rtv));

			DirectX::XMFLOAT4X4 worldToClip = getCameraForFace(i);
			GFX::Cmd::UploadToBuffer(context, cameraCB, 0, &worldToClip, 0, sizeof(DirectX::XMFLOAT4X4));

			context->OMSetRenderTargets(1, &rtv, nullptr);
			context->Draw(GFX::GetNumElements(CubeVB), 0);

			rtv->Release();
		}

		GFX::Storage::Free(shader);
		GFX::Storage::Free(cameraCB);

		GFX::Cmd::MarkerEnd(context);

		return cubemapTex;
	}

	void GenerateSkybox(ID3D11DeviceContext* context, TextureID& cubemap)
	{
		if (cubemap.Valid())
		{
			GFX::Storage::Free(cubemap);
		}
		TextureID skyboxPanorama = GFX::LoadTextureHDR("Resources/skybox_panorama.hdr", RCF_Bind_SRV);
		cubemap = PanoramaToCubemap(context, skyboxPanorama, 512);
		GFX::Storage::Free(skyboxPanorama);
	}

	void SetupShadowmapData(ID3D11DeviceContext* context)
	{
		bool foundDirLight = false;
		Float3 lightDirection;

		for (uint32_t i = 0; i < MainSceneGraph.Lights.GetSize(); i++)
		{
			Light& l = MainSceneGraph.Lights[i];
			if (l.Type == LT_Directional)
			{
				ASSERT(!foundDirLight, "Only one directional light is allowed per scene!");
				foundDirLight = true;
				lightDirection = l.Direction;
			}
		}
		ASSERT(foundDirLight, "No directional light found! Please add a directional light to the scene!");

		using namespace DirectX;
		Float3 camPos = MainSceneGraph.MainCamera.Position.ToXM();
		
		// TODO: Reduce number of variables used
		XMMATRIX view = XMMatrixLookAtLH(camPos.ToXM(), (camPos + lightDirection).ToXM(), Float3(0.0f, -1.0f, 0.0f).ToXM());
		//XMMATRIX proj = XMMatrixOrthographicLH(500.0f, 500.0f, -100.0f, 100.0f);
		XMMATRIX proj = XMMatrixOrthographicOffCenterLH(-100.0f, 100.0f, -100.0f, 100.0f, -1000.0f, 1000.0f);
		XMMATRIX viewProj = XMMatrixMultiply(view, proj);
		XMMATRIX viewProjFinal = XMMatrixTranspose(viewProj);

		XMFLOAT4X4 worldToLightClip;
		XMStoreFloat4x4(&worldToLightClip, viewProjFinal);

		GFX::Cmd::UploadToBuffer(context, MainSceneGraph.WorldToLightClip, 0, &worldToLightClip, 0, sizeof(XMFLOAT4X4));
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
		MainSceneGraph.MainCamera.Position += Float3(relativeDir.x, relativeDir.y, relativeDir.z);

		if (!Window::Get()->IsCursorShown())
		{
			Float2 mouseDelta = Input::GetMouseDelta();
			Float3& cameraRot = MainSceneGraph.MainCamera.Rotation;
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
		GUI::Get()->AddElement(new PositionInfoGUI());

		if constexpr (ENABLE_STATS)
			GUI::Get()->AddElement(new RenderStatsGUI());
	}

	MainSceneGraph.InitRenderData(context);
	
	// Prepare scene
	{
		MainSceneGraph.CreateAmbientLight(context, Float3(0.1f, 0.1f, 0.15f));
		MainSceneGraph.CreateDirectionalLight(context, Float3(-1.0f, -1.0f, -1.0f), Float3(0.2f, 0.2f, 0.23f));

		constexpr uint32_t NUM_LIGHTS = 1024;
		for (uint32_t i = 0; i < NUM_LIGHTS; i++)
		{
			Float3 position = Float3(200.0f, 100.0f, 200.0f) * Float3(Random::SNorm(), Random::SNorm(), Random::SNorm());
			Float3 color = Float3(Random::UNorm(), Random::UNorm(), Random::UNorm());
			Float2 falloff = Float2(1.0f + 3.0f * Random::UNorm(), 5.0f + 10.0f * Random::UNorm());
			MainSceneGraph.CreatePointLight(context, position, color, falloff);
		}

		if (AppConfig.Settings.contains("SIMPLE_SCENE"))
		{
			Entity& e1 = MainSceneGraph.CreateEntity(context, { 0.0f, -10.0f, 0.0f }, { 10000.0f, 1.0f, 10000.0f });
			Entity& e2 = MainSceneGraph.CreateEntity(context, { 10.0f, 0.0f, 10.0f });
			SceneLoading::LoadEntity("Resources/cube/cube.gltf", e1);
			SceneLoading::LoadEntity("Resources/cube/cube.gltf", e2);
		}
		else
		{
			Entity& e1 = MainSceneGraph.CreateEntity(context, { 0.0f, 0.0f, 0.0f }, { 0.1f, 0.1f, 0.1f } );
			Entity& e2 = MainSceneGraph.CreateEntity(context);
			
			SceneLoading::LoadEntityInBackground("Resources/sponza/sponza.gltf", e1);
			SceneLoading::LoadEntity("Resources/cube/cube.gltf", e2);
		}

		// Shadows
		MainSceneGraph.WorldToLightClip = GFX::CreateConstantBuffer<DirectX::XMFLOAT4X4>();
		MainSceneGraph.ShadowMapTexture = GFX::CreateTexture(512, 512, RCF_Bind_DSV | RCF_Bind_SRV);

		static const float vbData[] = {
		-1.0f,-1.0f,-1.0f,
		-1.0f,-1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f,-1.0f,
		-1.0f,-1.0f,-1.0f,
		-1.0f, 1.0f,-1.0f,
		1.0f,-1.0f, 1.0f,
		-1.0f,-1.0f,-1.0f,
		1.0f,-1.0f,-1.0f,
		1.0f, 1.0f,-1.0f,
		1.0f,-1.0f,-1.0f,
		-1.0f,-1.0f,-1.0f,
		-1.0f,-1.0f,-1.0f,
		-1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f,-1.0f,
		1.0f,-1.0f, 1.0f,
		-1.0f,-1.0f, 1.0f,
		-1.0f,-1.0f,-1.0f,
		-1.0f, 1.0f, 1.0f,
		-1.0f,-1.0f, 1.0f,
		1.0f,-1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f,-1.0f,-1.0f,
		1.0f, 1.0f,-1.0f,
		1.0f,-1.0f,-1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f,-1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f,-1.0f,
		-1.0f, 1.0f,-1.0f,
		1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f,-1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f,-1.0f, 1.0f
		};
		static uint32_t numVertices = STATIC_ARRAY_SIZE(vbData) / 3;

		CubeVB = GFX::CreateVertexBuffer<Float3>(numVertices, (Float3*)vbData);
	}

	m_SkyboxShader = GFX::CreateShader("Source/Shaders/skybox.hlsl");
	m_ShadowmapShader = GFX::CreateShader("Source/Shaders/shadowmap.hlsl", {}, SCF_VS);
	m_DepthPrepassShader = GFX::CreateShader("Source/Shaders/depth.hlsl", {}, SCF_VS);
	m_GeometryShader = GFX::CreateShader("Source/Shaders/geometry.hlsl");
	m_GeometryShaderNoLightCulling = GFX::CreateShader("Source/Shaders/geometry.hlsl", { "DISABLE_LIGHT_CULLING" });
	m_GeometryAlphaDiscardShader = GFX::CreateShader("Source/Shaders/geometry.hlsl", { "ALPHA_DISCARD" });
	m_GeometryAlphaDiscardShaderNoLightCulling = GFX::CreateShader("Source/Shaders/geometry.hlsl", { "ALPHA_DISCARD", "DISABLE_LIGHT_CULLING" });
	m_LightCullingShader = GFX::CreateShader("Source/Shaders/light_culling.hlsl", { "USE_BARRIERS"}, SCF_CS);
	m_DebugGeometryShader = GFX::CreateShader("Source/Shaders/debug_geometry.hlsl");

	GenerateSkybox(context, m_SkyboxCubemap);
	m_FinalRT = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV);
	m_FinalRT_Depth = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_DSV | RCF_Bind_SRV);

	m_IndexBuffer = GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_IB | RCF_CopyDest);

	UpdateCullingResources(context);

	if constexpr (ENABLE_STATS)
	{
		m_LightStatsShader = GFX::CreateShader("Source/Shaders/light_stats.hlsl", {}, SCF_CS);
		m_LightStatsBuffer = GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_SB | RCF_Bind_UAV | RCF_CPU_Read);
	}
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
	bv.Center = e.Position + d.BoundingVolume.Center;
	bv.Radius = d.BoundingVolume.Radius * 0.5f * maxScale;

	return vf.IsInFrustum(bv);
}

BitField CullDrawables()
{
	const uint32_t numDrawables = MainSceneGraph.Drawables.GetSize();
	BitField visibilityMask{ numDrawables };
	for (uint32_t i = 0; i < numDrawables; i++)
	{
		Drawable& d = MainSceneGraph.Drawables[i];
		if (d.DrawableIndex != Drawable::InvalidIndex && IsVisible(d))
		{
			visibilityMask.Set(i, true);
		}
	}
	return visibilityMask;
}

template<typename FilterFunc>
BitField FilterVisibilityMask(BitField& visibilityMask, FilterFunc filterFunc)
{
	const uint32_t numDrawables = MainSceneGraph.Drawables.GetSize();
	BitField filteredMask{ numDrawables };
	for (uint32_t i = 0; i < numDrawables; i++)
	{
		Drawable& d = MainSceneGraph.Drawables[i];
		if (visibilityMask.Get(i) && filterFunc(d))
		{
			filteredMask.Set(i, true);
		}
	}
	return filteredMask;
}

uint32_t PrepareIndexBuffer(ID3D11DeviceContext* context, BufferID indexBuffer, const BitField& visibilityMask)
{
	std::vector<uint32_t> indices;
	indices.resize(MainSceneGraph.Geometries.GetIndexCount());

	uint32_t indexCount = 0;
	for (uint32_t i = 0; i < MainSceneGraph.Drawables.GetSize(); i++)
	{
		if (visibilityMask.Get(i))
		{
			const Mesh& mesh = MainSceneGraph.Meshes[MainSceneGraph.Drawables[i].MeshIndex];
			
			uint8_t* copySrc = reinterpret_cast<uint8_t*>(MainSceneGraph.Geometries.GetIndexBuffer().data());
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

	if (!DebugToolsConfig.FreezeGeometryCulling)
	{
		if (DebugToolsConfig.DisableGeometryCulling)
		{
			uint32_t numDrawables = MainSceneGraph.Drawables.GetSize();
			m_VisibilityMask = BitField(numDrawables);
			for (uint32_t i = 0; i < numDrawables; i++) m_VisibilityMask.Set(i, true);
		}
		else
		{
			m_VisibilityMask = CullDrawables();
		}
	}

	GFX::Cmd::ClearRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
	GFX::Cmd::BindRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
	
	// Skybox
	{
		GFX::Cmd::MarkerBegin(context, "Skybox");
		GFX::Cmd::BindShader(context, m_SkyboxShader);
		GFX::Cmd::BindVertexBuffer(context, CubeVB);
		GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindSRV<PS>(context, m_SkyboxCubemap, 0);
		GFX::Cmd::SetupStaticSamplers<PS>(context);
		context->Draw(GFX::GetNumElements(CubeVB), 0);
		GFX::Cmd::MarkerEnd(context);
	}

	// Depth prepass
	{
		GFX::Cmd::MarkerBegin(context, "Depth Prepass");
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;

		const MeshStorage& meshStorage = MainSceneGraph.Geometries;
		const auto drawableFilter = [](const Drawable& d)
		{
			Material& mat = MainSceneGraph.Materials[d.MaterialIndex];
			return !mat.UseAlphaDiscard && !mat.UseBlend;
		};
		const BitField filteredMask = FilterVisibilityMask(m_VisibilityMask, drawableFilter);
		const uint32_t indexCount = PrepareIndexBuffer(context, m_IndexBuffer, filteredMask);

		GFX::Cmd::BindRenderTarget(context, TextureID{}, m_FinalRT_Depth);
		GFX::Cmd::SetPipelineState(context, pso);
		GFX::Cmd::BindShader(context, m_DepthPrepassShader, true);
		GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Entities.GetBuffer(), 0);
		GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Drawables.GetBuffer(), 1);
		GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetDrawableIndexes() });
		GFX::Cmd::BindIndexBuffer(context, m_IndexBuffer);
		context->DrawIndexed(indexCount, 0, 0);
		GFX::Cmd::MarkerEnd(context);
	}
	
	// Light culling
	if(!DebugToolsConfig.FreezeLightCulling && !DebugToolsConfig.DisableLightCulling)
	{
		context->OMSetRenderTargets(0, nullptr, nullptr);
		GFX::Cmd::MarkerBegin(context, "Light Culling");
		GFX::Cmd::BindShader(context, m_LightCullingShader);
		GFX::Cmd::BindCBV<CS>(context, MainSceneGraph.SceneInfoBuffer, 0);
		GFX::Cmd::BindCBV<CS>(context, m_TileCullingInfoBuffer, 1);
		GFX::Cmd::BindCBV<CS>(context, MainSceneGraph.MainCamera.CameraBuffer, 2);
		GFX::Cmd::BindSRV<CS>(context, MainSceneGraph.Lights.GetBuffer(), 0);
		GFX::Cmd::BindSRV<CS>(context, m_FinalRT_Depth, 1);
		GFX::Cmd::BindUAV<CS>(context, m_VisibleLightsBuffer, 0);
		context->Dispatch(m_NumTilesX, m_NumTilesY, 1);
		GFX::Cmd::BindUAV<CS>(context, nullptr, 0);
		GFX::Cmd::BindSRV<CS>(context, nullptr, 1);
		GFX::Cmd::MarkerEnd(context);
	}

	// Geometry
	{
		GFX::Cmd::MarkerBegin(context, "Geometry");
		{
			PipelineState pso = GFX::DefaultPipelineState();
			pso.DS.DepthEnable = true;
			pso.DS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			GFX::Cmd::SetPipelineState(context, pso);
		}

		const bool useLightCulling = !DebugToolsConfig.DisableLightCulling;
		{
			const MeshStorage& meshStorage = MainSceneGraph.Geometries;
			const auto drawableFilter = [](const Drawable& d)
			{
				Material& mat = MainSceneGraph.Materials[d.MaterialIndex];
				return !mat.UseAlphaDiscard && !mat.UseBlend;
			};
			const BitField filteredMask = FilterVisibilityMask(m_VisibilityMask, drawableFilter);
			const uint32_t indexCount = PrepareIndexBuffer(context, m_IndexBuffer, filteredMask);

			GFX::Cmd::BindRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
			GFX::Cmd::BindShader(context, useLightCulling ? m_GeometryShader : m_GeometryShaderNoLightCulling, true);
			GFX::Cmd::SetupStaticSamplers<PS>(context);
			GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Textures, 0);
			GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
			GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
			GFX::Cmd::BindCBV<PS>(context, useLightCulling ? m_TileCullingInfoBuffer : MainSceneGraph.SceneInfoBuffer, 1);
			GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.WorldToLightClip, 3);
			GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Lights.GetBuffer(), 3);
			GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.ShadowMapTexture, 4);
			GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Entities.GetBuffer(), 5);
			GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Materials.GetBuffer(), 6);
			GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Drawables.GetBuffer(), 7);
			GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Drawables.GetBuffer(), 7);
			GFX::Cmd::BindSRV<PS>(context, m_VisibleLightsBuffer, 8);
			GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetTexcoords(), meshStorage.GetNormals(), meshStorage.GetTangents(), meshStorage.GetDrawableIndexes() });
			GFX::Cmd::BindIndexBuffer(context, m_IndexBuffer);
			context->DrawIndexed(indexCount, 0, 0);
		}
		
		{
			const MeshStorage& meshStorage = MainSceneGraph.Geometries;
			const auto drawableFilter = [](const Drawable& d)
			{
				Material& mat = MainSceneGraph.Materials[d.MaterialIndex];
				return mat.UseAlphaDiscard && !mat.UseBlend;
			};
			const BitField filteredMask = FilterVisibilityMask(m_VisibilityMask, drawableFilter);
			const uint32_t indexCount = PrepareIndexBuffer(context, m_IndexBuffer, filteredMask);

			GFX::Cmd::BindShader(context, useLightCulling ? m_GeometryAlphaDiscardShader : m_GeometryAlphaDiscardShaderNoLightCulling, true);
			GFX::Cmd::BindIndexBuffer(context, m_IndexBuffer);
			context->DrawIndexed(indexCount, 0, 0);
		}

		GFX::Cmd::MarkerEnd(context);
	}

	// Debug geometries
	if(DebugToolsConfig.DrawBoundingBoxes)
	{
		const uint32_t numDrawables = MainSceneGraph.Drawables.GetSize();
		for (uint32_t i = 0; i < numDrawables; i++)
		{
			if (!m_VisibilityMask.Get(i)) continue;

			const Drawable& d = MainSceneGraph.Drawables[i];
			const Entity& e = MainSceneGraph.Entities[d.EntityIndex];
			const float maxScale = MAX(MAX(e.Scale.x, e.Scale.y), e.Scale.z);
			
			BoundingSphere bs;
			bs.Center = e.Position + d.BoundingVolume.Center;
			bs.Radius = d.BoundingVolume.Radius * 0.5f * maxScale;

			DebugGeometry dg{};
			dg.Color = Float4(Random::UNorm(i), Random::UNorm(i+1), Random::UNorm(i+2), 0.2f);
			dg.Position = bs.Center;
			dg.Scale = bs.Radius;
			m_DebugGeometries.push_back(dg);
		}

		DrawDebugGeometries(context);
	}


	if constexpr (ENABLE_STATS)
		UpdateStats(context);

	return m_FinalRT;
}

void ForwardPlus::OnUpdate(ID3D11DeviceContext* context, float dt)
{
	using namespace ForwardPlusPrivate;
	UpdateInput(dt, this);
}

void ForwardPlus::OnShaderReload(ID3D11DeviceContext* context)
{
	using namespace ForwardPlusPrivate;

	GenerateSkybox(context, m_SkyboxCubemap);
}

void ForwardPlus::OnWindowResize(ID3D11DeviceContext* context)
{
	GFX::Storage::Free(m_FinalRT);
	GFX::Storage::Free(m_FinalRT_Depth);
	m_FinalRT = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV);
	m_FinalRT_Depth = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_DSV | RCF_Bind_SRV);

	UpdateCullingResources(context);
}

void ForwardPlus::UpdateCullingResources(ID3D11DeviceContext* context)
{
	struct TileCullingInfoCB
	{
		uint32_t ScreenSizeX;
		uint32_t ScreenSizeY;
		uint32_t NumTilesX;
		uint32_t NumTilesY;
	};

	if(m_VisibleLightsBuffer.Valid())
		GFX::Storage::Free(m_VisibleLightsBuffer);

	if (!m_TileCullingInfoBuffer.Valid())
		m_TileCullingInfoBuffer = GFX::CreateConstantBuffer<TileCullingInfoCB>();

	m_NumTilesX = MathUtility::CeilDiv(AppConfig.WindowWidth, TILE_SIZE);
	m_NumTilesY = MathUtility::CeilDiv(AppConfig.WindowHeight, TILE_SIZE);
	m_VisibleLightsBuffer = GFX::CreateBuffer(m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_SB | RCF_Bind_UAV);

	TileCullingInfoCB tileCullingInfoCB{};
	tileCullingInfoCB.ScreenSizeX = AppConfig.WindowWidth;
	tileCullingInfoCB.ScreenSizeY = AppConfig.WindowHeight;
	tileCullingInfoCB.NumTilesX = m_NumTilesX;
	tileCullingInfoCB.NumTilesY = m_NumTilesY;

	GFX::Cmd::UploadToBuffer(context, m_TileCullingInfoBuffer, 0, &tileCullingInfoCB, 0, sizeof(TileCullingInfoCB));
}

void ForwardPlus::UpdateStats(ID3D11DeviceContext* context)
{
	GFX::Cmd::MarkerBegin(context, "Update stats");

	// Drawable stats
	RenderStats.TotalDrawables = MainSceneGraph.Drawables.GetSize();
	if (!DebugToolsConfig.FreezeGeometryCulling)
	{
		RenderStats.VisibleDrawables = m_VisibilityMask.CountOnes();
	}
	
	// Light stats
	RenderStats.TotalLights = MainSceneGraph.Lights.GetSize();

	if (DebugToolsConfig.DisableLightCulling)
	{
		RenderStats.VisibleLights = RenderStats.TotalLights;
	}
	else if (!DebugToolsConfig.FreezeLightCulling)
	{
#ifdef LIGHT_STATS // Tmp disabled - too slow
		GFX::Cmd::BindShader(context, m_LightStatsShader);
		GFX::Cmd::BindUAV<CS>(context, m_LightStatsBuffer, 0);
		GFX::Cmd::BindSRV<CS>(context, m_VisibleLightsBuffer, 0);
		GFX::Cmd::BindCBV<CS>(context, m_TileCullingInfoBuffer, 0);
		context->Dispatch(1, 1, 1);

		D3D11_MAPPED_SUBRESOURCE mapResult;
		const Buffer& lightStatsBuffer = GFX::Storage::GetBuffer(m_LightStatsBuffer);
		API_CALL(context->Map(lightStatsBuffer.Handle.Get(), 0, D3D11_MAP_READ, 0, &mapResult));

		uint32_t* dataPtr = reinterpret_cast<uint32_t*>(mapResult.pData);
		RenderStats.VisibleLights = *dataPtr;

		context->Unmap(lightStatsBuffer.Handle.Get(), 0);
#else
		RenderStats.VisibleLights = RenderStats.TotalLights;
#endif
		
	}

	GFX::Cmd::MarkerEnd(context);
}

void ForwardPlus::DrawDebugGeometries(ID3D11DeviceContext* context)
{
	struct DebugGeometryDataCB
	{
		DirectX::XMFLOAT4X4 ModelToWorld;
		DirectX::XMFLOAT4 Color;
	};

	if (!m_DebugGeometryBuffer.Valid()) m_DebugGeometryBuffer = GFX::CreateConstantBuffer<DebugGeometryDataCB>();

	GFX::Cmd::MarkerBegin(context, "Debug Geometries");

	// Prepare pipeline
	PipelineState pso = GFX::DefaultPipelineState();
	pso.DS.DepthEnable = true;
	pso.DS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	pso.BS.RenderTarget[0].BlendEnable = true;
	pso.BS.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	pso.BS.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
	pso.BS.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	pso.BS.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	pso.BS.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	pso.BS.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	GFX::Cmd::SetPipelineState(context, pso);
	GFX::Cmd::BindShader(context, m_DebugGeometryShader);
	GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 1);

	for (const DebugGeometry& dg : m_DebugGeometries)
	{
		// Prepare constant buffer
		{
			using namespace DirectX;
			Float3 scale{ dg.Scale, dg.Scale, dg.Scale };
			XMMATRIX modelToWorld = XMMatrixTranspose(XMMatrixAffineTransformation(scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), dg.Position.ToXM()));

			DebugGeometryDataCB debugGeometryDataCB{};
			debugGeometryDataCB.ModelToWorld = XMUtility::ToXMFloat4x4(modelToWorld);
			debugGeometryDataCB.Color = dg.Color.ToXMF();
			GFX::Cmd::UploadToBuffer(context, m_DebugGeometryBuffer, 0, &debugGeometryDataCB, 0, sizeof(DebugGeometryDataCB));
		}

		// Draw
		{
			BufferID vb = ForwardPlusPrivate::CubeVB;
			GFX::Cmd::BindVertexBuffer(context, vb);
			GFX::Cmd::BindCBV<VS>(context, m_DebugGeometryBuffer, 0);
			GFX::Cmd::BindCBV<PS>(context, m_DebugGeometryBuffer, 0);
			context->Draw(GFX::GetNumElements(vb), 0);
		}
	}

	GFX::Cmd::SetPipelineState(context, GFX::DefaultPipelineState());

	GFX::Cmd::MarkerEnd(context);

	m_DebugGeometries.clear();
}