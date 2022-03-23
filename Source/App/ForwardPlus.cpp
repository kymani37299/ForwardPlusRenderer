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
#include "System/Input.h"
#include "System/Window.h"
#include "System/ApplicationConfiguration.h"
#include "Loading/SceneLoading.h"

namespace ForwardPlusPrivate
{
	BufferID CubeVB;

	// [0,1]
	float Rand()
	{
		return (float)rand() / RAND_MAX;
	}

	// [-1,1]
	float Rand2()
	{
		return Rand() * 2.0f - 1.0f;
	}

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
		for (const Light& l : MainSceneGraph.Lights)
		{
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
			Window::Get()->ShowCursor(GUI::Get()->ToggleVisible());
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
		GUI::Get()->AddElement(new FPSCounterGUI());
	}

	
	// Prepare scene
	{
		MainSceneGraph.Lights.push_back(Light::CreateAmbient(Float3(0.1f, 0.1f, 0.15f)));
		MainSceneGraph.Lights.push_back(Light::CreateDirectional(Float3(-1.0f, -1.0f, -1.0f), Float3(0.2f, 0.2f, 0.23f)));

		constexpr uint32_t NUM_LIGHTS = 1024;
		for (uint32_t i = 0; i < NUM_LIGHTS; i++)
		{
			Float3 position = Float3(200.0f, 100.0f, 200.0f) * Float3(Rand2(), Rand2(), Rand2());
			Float3 color = Float3(Rand(), Rand(), Rand());
			Float2 falloff = Float2(1.0f + 3.0f * Rand(), 5.0f + 10.0f * Rand());
			MainSceneGraph.Lights.push_back(Light::CreatePoint(position, color, falloff));
		}

		MainSceneGraph.UpdateRenderData(context);

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

		// TODO: Add new renderpass for this
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
	m_GeometryAlphaDiscardShader = GFX::CreateShader("Source/Shaders/geometry.hlsl", { "ALPHA_DISCARD" });

	GenerateSkybox(context, m_SkyboxCubemap);
	m_FinalRT = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV);
	m_FinalRT_Depth = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_DSV);
}

void ForwardPlus::OnDestroy(ID3D11DeviceContext* context)
{

}

TextureID ForwardPlus::OnDraw(ID3D11DeviceContext* context)
{
	using namespace ForwardPlusPrivate;

	GFX::Cmd::ClearRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
	GFX::Cmd::BindRenderTarget(context, m_FinalRT, m_FinalRT_Depth);

	// Prepare scene
	{
		MainSceneGraph.MainCamera.UpdateBuffer(context);
		MainSceneGraph.UpdateDrawables(context);
	}
	
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

	// Shadowmap
	//{
	//	GFX::Cmd::MarkerBegin(context, "Shadowmap");
	//	SetupShadowmapData(context);
	//
	//	PipelineState pso = GFX::DefaultPipelineState();
	//	pso.DS.DepthEnable = true;
	//
	//	GFX::Cmd::SetPipelineState(context, pso);
	//	GFX::Cmd::BindShader(context, m_ShadowmapShader, true);
	//	GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.WorldToLightClip, 0);
	//	GFX::Cmd::ClearRenderTarget(context, TextureID{}, MainSceneGraph.ShadowMapTexture);
	//	GFX::Cmd::BindRenderTarget(context, TextureID{}, MainSceneGraph.ShadowMapTexture);
	//
	//	for (Entity& e : MainSceneGraph.Entities)
	//	{
	//		GFX::Cmd::BindCBV<VS>(context, e.EntityBuffer, 1);
	//
	//		const auto func = [&context](const Drawable& d) {
	//			if (!d.Material.UseAlphaDiscard && !d.Material.UseBlend)
	//			{
	//				const Mesh& m = d.Mesh;
	//				GFX::Cmd::BindVertexBuffer(context, m.Position);
	//				GFX::Cmd::BindIndexBuffer(context, m.Indices);
	//				context->DrawIndexed(GFX::GetNumElements(m.Indices), 0, 0);
	//			}
	//
	//		};
	//		e.Drawables.ForEach(func);
	//	}
	//	GFX::Cmd::MarkerEnd(context);
	//}

	// Depth prepass
	{
		GFX::Cmd::MarkerBegin(context, "Depth prepass");
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;

		const MeshStorage& meshStorage = MainSceneGraph.OpaqueGeometries;
		GFX::Cmd::BindRenderTarget(context, TextureID{}, m_FinalRT_Depth);
		GFX::Cmd::SetPipelineState(context, pso);
		GFX::Cmd::BindShader(context, m_DepthPrepassShader, true);
		GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Entities.GetBuffer(), 0);
		GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetDrawableIndexes() });
		GFX::Cmd::BindIndexBuffer(context, meshStorage.GetIndexBuffer());
		context->DrawIndexed(meshStorage.GetIndexCount(), 0, 0);
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

		{
			const MeshStorage& meshStorage = MainSceneGraph.OpaqueGeometries;
			GFX::Cmd::BindRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
			GFX::Cmd::BindShader(context, m_GeometryShader, true);
			GFX::Cmd::SetupStaticSamplers<PS>(context);
			GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Textures, 0);
			GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
			GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
			GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.WorldToLightClip, 3);
			GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.LightsBuffer, 3);
			GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.ShadowMapTexture, 4);
			GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Entities.GetBuffer(), 5);
			GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Materials.GetBuffer(), 6);
			GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetTexcoords(), meshStorage.GetNormals(), meshStorage.GetTangents(), meshStorage.GetDrawableIndexes() });
			GFX::Cmd::BindIndexBuffer(context, meshStorage.GetIndexBuffer());
			context->DrawIndexed(meshStorage.GetIndexCount(), 0, 0);
		}
		
		{
			const MeshStorage& meshStorage = MainSceneGraph.AlphaDiscardGeometries;
			GFX::Cmd::BindShader(context, m_GeometryAlphaDiscardShader, true);
			GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetTexcoords(), meshStorage.GetNormals(), meshStorage.GetTangents(), meshStorage.GetDrawableIndexes() });
			GFX::Cmd::BindIndexBuffer(context, meshStorage.GetIndexBuffer());
			context->DrawIndexed(meshStorage.GetIndexCount(), 0, 0);
		}

		GFX::Cmd::MarkerEnd(context);
	}

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
	m_FinalRT_Depth = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_DSV);
}