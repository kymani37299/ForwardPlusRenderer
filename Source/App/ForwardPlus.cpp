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
	BufferID CubeVB;
	BufferID SphereVB;
	BufferID PlaneVB;

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

	BufferID GenerateSphereVB()
	{
		constexpr uint32_t parallels = 11;
		constexpr uint32_t meridians = 22;
		constexpr float PI = 3.14159265358979323846;

		std::vector<Float3> verticesRaw;
		std::vector<Float3> vertices;

		verticesRaw.push_back({ 0.0f, 1.0f, 0.0f });
		for (uint32_t j = 0; j < parallels - 1; ++j)
		{
			float polar = PI * float(j + 1) / float(parallels);
			float sp = std::sin(polar);
			float cp = std::cos(polar);
			for (uint32_t i = 0; i < meridians; ++i)
			{
				float azimuth = 2.0 * PI * float(i) / float(meridians);
				float sa = std::sin(azimuth);
				float ca = std::cos(azimuth);
				float x = sp * ca;
				float y = cp;
				float z = sp * sa;
				verticesRaw.push_back({ x, y, z });
			}
		}
		verticesRaw.push_back({ 0.0f, -1.0f, 0.0f });

		for (uint32_t i = 0; i < meridians; ++i)
		{
			uint32_t const a = i + 1;
			uint32_t const b = (i + 1) % meridians + 1;
			vertices.push_back(verticesRaw[0]);
			vertices.push_back(verticesRaw[b]);
			vertices.push_back(verticesRaw[a]);
		}

		for (uint32_t j = 0; j < parallels - 2; ++j)
		{
			uint32_t aStart = j * meridians + 1;
			uint32_t bStart = (j + 1) * meridians + 1;
			for (uint32_t i = 0; i < meridians; ++i)
			{
				const uint32_t a = aStart + i;
				const uint32_t a1 = aStart + (i + 1) % meridians;
				const uint32_t b = bStart + i;
				const uint32_t b1 = bStart + (i + 1) % meridians;
				vertices.push_back(verticesRaw[a]);
				vertices.push_back(verticesRaw[a1]);
				vertices.push_back(verticesRaw[b1]);
				vertices.push_back(verticesRaw[b]);
				vertices.push_back(verticesRaw[a]);
				vertices.push_back(verticesRaw[b1]);
			}
		}

		for (uint32_t i = 0; i < meridians; ++i)
		{
			uint32_t const a = i + meridians * (parallels - 2) + 1;
			uint32_t const b = (i + 1) % meridians + meridians * (parallels - 2) + 1;
			vertices.push_back(verticesRaw[verticesRaw.size()-1]);
			vertices.push_back(verticesRaw[a]);
			vertices.push_back(verticesRaw[b]);
		}

		return GFX::CreateVertexBuffer<Float3>(vertices.size(), vertices.data());
	}

	BufferID GenerateCubeVB()
	{
		static const float vbData[] = { -1.0f,-1.0f,-1.0f,-1.0f,-1.0f, 1.0f,-1.0f, 1.0f, 1.0f,1.0f, 1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f, 1.0f,-1.0f,1.0f,-1.0f, 1.0f,-1.0f,-1.0f,-1.0f,1.0f,-1.0f,-1.0f,1.0f, 1.0f,-1.0f,1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f, 1.0f, 1.0f,-1.0f, 1.0f,-1.0f,1.0f,-1.0f, 1.0f,-1.0f,-1.0f, 1.0f,-1.0f,-1.0f,-1.0f,-1.0f, 1.0f, 1.0f,-1.0f,-1.0f, 1.0f,1.0f,-1.0f, 1.0f,1.0f, 1.0f, 1.0f,1.0f,-1.0f,-1.0f,1.0f, 1.0f,-1.0f,1.0f,-1.0f,-1.0f,1.0f, 1.0f, 1.0f,1.0f,-1.0f, 1.0f,1.0f, 1.0f, 1.0f,1.0f, 1.0f,-1.0f,-1.0f, 1.0f,-1.0f,1.0f, 1.0f, 1.0f,-1.0f, 1.0f,-1.0f,-1.0f, 1.0f, 1.0f,1.0f, 1.0f, 1.0f,-1.0f, 1.0f, 1.0f,1.0f,-1.0f, 1.0f };
		static uint32_t numVertices = STATIC_ARRAY_SIZE(vbData) / 3;
		return GFX::CreateVertexBuffer<Float3>(numVertices, (Float3*)vbData);
	}

	BufferID GeneratePlaneVB()
	{
		static const float vbData[] = { 1.0,0.0,1.0,-1.0,0.0,-1.0,1.0,0.0,-1.0,1.0,0.0,1.0,-1.0,0.0,1.0,-1.0,0.0,-1.0 };
		static uint32_t numVertices = STATIC_ARRAY_SIZE(vbData) / 3;
		return GFX::CreateVertexBuffer<Float3>(numVertices, (Float3*)vbData);
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
		GUI::Get()->AddElement(new DebugToolsGUI(true));
		GUI::Get()->AddElement(new PostprocessingGUI());
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
			Entity& plane = MainSceneGraph.CreateEntity(context, { 0.0f, -10.0f, 0.0f }, { 10000.0f, 1.0f, 10000.0f });
			SceneLoading::LoadEntity("Resources/cube/cube.gltf", plane);

			constexpr uint32_t NUM_CUBES = 50;
			for (uint32_t i = 0; i < NUM_CUBES; i++)
			{
				const Float3 position = Float3{ Random::SNorm(), Random::SNorm(), Random::SNorm() } * Float3{ 100.0f, 100.0f, 100.0f};
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

		CubeVB = GenerateCubeVB();
		SphereVB = GenerateSphereVB();
		PlaneVB = GeneratePlaneVB();
	}

	m_SkyboxShader = GFX::CreateShader("Source/Shaders/skybox.hlsl");
	m_DepthPrepassShader = GFX::CreateShader("Source/Shaders/depth.hlsl");
	m_GeometryShader = GFX::CreateShader("Source/Shaders/geometry.hlsl");
	m_GeometryShaderNoLightCulling = GFX::CreateShader("Source/Shaders/geometry.hlsl", { "DISABLE_LIGHT_CULLING" });
	m_GeometryAlphaDiscardShader = GFX::CreateShader("Source/Shaders/geometry.hlsl", { "ALPHA_DISCARD" });
	m_GeometryAlphaDiscardShaderNoLightCulling = GFX::CreateShader("Source/Shaders/geometry.hlsl", { "ALPHA_DISCARD", "DISABLE_LIGHT_CULLING" });
	m_LightCullingShader = GFX::CreateShader("Source/Shaders/light_culling.hlsl", { "USE_BARRIERS"}, SCF_CS);
	m_DebugGeometryShader = GFX::CreateShader("Source/Shaders/debug_geometry.hlsl");
	m_LightHeatmapShader = GFX::CreateShader("Source/Shaders/light_heatmap.hlsl");
	m_TAA = GFX::CreateShader("Source/Shaders/taa.hlsl");

	GenerateSkybox(context, m_SkyboxCubemap);
	m_IndexBuffer = GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_IB | RCF_CopyDest);

	UpdatePresentResources(context);
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

	if (!DebugToolsConfig.FreezeGeometryCulling)
	{
		for (uint32_t i = 0; i < RenderGroupType::Count; i++)
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

	GFX::Cmd::ClearRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
	GFX::Cmd::BindRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
	
	// Depth prepass
	{
		GFX::Cmd::MarkerBegin(context, "Depth Prepass");
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;

		RenderGroup& renderGroup = MainSceneGraph.RenderGroups[RenderGroupType::Opaque];
		const MeshStorage& meshStorage = renderGroup.MeshData;
		const uint32_t indexCount = PrepareIndexBuffer(context, m_IndexBuffer, renderGroup);

		GFX::Cmd::BindRenderTarget(context, m_MotionVectorRT, m_FinalRT_Depth);
		GFX::Cmd::SetPipelineState(context, pso);
		GFX::Cmd::BindShader(context, m_DepthPrepassShader, true);
		GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
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
		context->OMSetRenderTargets(0, nullptr, nullptr);
		GFX::Cmd::MarkerBegin(context, "Light Culling");
		GFX::Cmd::BindShader(context, m_LightCullingShader);
		GFX::Cmd::BindCBV<CS>(context, MainSceneGraph.SceneInfoBuffer, 0);
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
		
		const auto rgTypeToShader = [this](RenderGroupType rgType) {
			const bool useLightCulling = !DebugToolsConfig.DisableLightCulling;
			switch (rgType)
			{
			case RenderGroupType::Opaque:
				return useLightCulling ? m_GeometryShader : m_GeometryShaderNoLightCulling;
			case RenderGroupType::AlphaDiscard:
				return useLightCulling ? m_GeometryAlphaDiscardShader : m_GeometryAlphaDiscardShaderNoLightCulling;
			default:
				NOT_IMPLEMENTED;
			}
		};

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

		GFX::Cmd::BindRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
		GFX::Cmd::SetupStaticSamplers<PS>(context);
		GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.SceneInfoBuffer, 1);
		GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.Lights.GetBuffer(), 3);
		GFX::Cmd::BindSRV<VS>(context, MainSceneGraph.Entities.GetBuffer(), 5);
		GFX::Cmd::BindSRV<PS>(context, m_VisibleLightsBuffer, 8);
		GFX::Cmd::BindIndexBuffer(context, m_IndexBuffer);

		for (uint32_t i = 0; i < RenderGroupType::Count; i++)
		{
			RenderGroupType rgType = (RenderGroupType) i;
			RenderGroup& renderGroup = MainSceneGraph.RenderGroups[i];
			if (renderGroup.Drawables.GetSize() == 0) continue;

			const MeshStorage& meshStorage = renderGroup.MeshData;
			const uint32_t indexCount = PrepareIndexBuffer(context, m_IndexBuffer, renderGroup);

			GFX::Cmd::BindShader(context, rgTypeToShader(rgType), true);
			GFX::Cmd::BindSRV<PS>(context, renderGroup.TextureData, 0);
			GFX::Cmd::BindSRV<PS>(context, renderGroup.Materials.GetBuffer(), 6);
			GFX::Cmd::BindSRV<VS>(context, renderGroup.Drawables.GetBuffer(), 7);
			GFX::Cmd::BindSRV<PS>(context, renderGroup.Drawables.GetBuffer(), 7);
			GFX::Cmd::BindVertexBuffers(context, { meshStorage.GetPositions(), meshStorage.GetTexcoords(), meshStorage.GetNormals(), meshStorage.GetTangents(), meshStorage.GetDrawableIndexes() });
			context->DrawIndexed(indexCount, 0, 0);
		}

		GFX::Cmd::MarkerEnd(context);
	}

	// Postprocessing
	{
		GFX::Cmd::MarkerBegin(context, "Postprocessing");

		// Prepare last frame
		GFX::Cmd::CopyToTexture(context, m_FinalRTHistory[1], m_FinalRTHistory[0]);
		GFX::Cmd::CopyToTexture(context, m_FinalRT, m_FinalRTHistory[1]);

		// TAA
		if (PostprocessSettings.EnableTAA)
		{
			GFX::Cmd::MarkerBegin(context, "TAA");

			// NOTE: TAA only works with static geometries now
			// In order for it to work with dynamic we need to store last frame model matrix so we calculate motion vectors good
			GFX::Cmd::CopyToTexture(context, m_FinalRT, m_FinalRTSRV);

			GFX::Cmd::BindRenderTarget(context, m_FinalRT);
			GFX::Cmd::BindShader(context, m_TAA);
			GFX::Cmd::SetupStaticSamplers<PS>(context);
			GFX::Cmd::BindSRV<PS>(context, m_FinalRTSRV, 0);
			GFX::Cmd::BindSRV<PS>(context, m_FinalRTHistory[0], 1);
			GFX::Cmd::BindSRV<PS>(context, m_MotionVectorRT, 2);
			GFX::Cmd::BindVertexBuffer(context, Device::Get()->GetQuadBuffer());
			context->Draw(6, 0);
			GFX::Cmd::MarkerEnd(context);
		}

		GFX::Cmd::MarkerEnd(context);
	}

	// Skybox
	{
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.StencilEnable = true;
		pso.DS.StencilWriteMask = 0;
		pso.DS.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		pso.DS.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		pso.DS.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		pso.DS.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		pso.DS.BackFace = pso.DS.FrontFace;

		GFX::Cmd::MarkerBegin(context, "Skybox");
		GFX::Cmd::SetPipelineState(context, pso);
		GFX::Cmd::BindRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
		GFX::Cmd::BindShader(context, m_SkyboxShader);
		GFX::Cmd::BindVertexBuffer(context, CubeVB);
		GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
		GFX::Cmd::BindSRV<PS>(context, m_SkyboxCubemap, 0);
		GFX::Cmd::SetupStaticSamplers<PS>(context);
		context->Draw(GFX::GetNumElements(CubeVB), 0);
		GFX::Cmd::MarkerEnd(context);
	}

	// Debug geometries
	if(DebugToolsConfig.DrawBoundingBoxes)
	{
		for (uint32_t rgType = 0; rgType < RenderGroupType::Count; rgType++)
		{
			RenderGroup& rg = MainSceneGraph.RenderGroups[rgType];
			const uint32_t numDrawables = rg.Drawables.GetSize();
			for (uint32_t i = 0; i < numDrawables; i++)
			{
				if (!rg.VisibilityMask.Get(i)) continue;

				const Drawable& d = rg.Drawables[i];
				const Entity& e = MainSceneGraph.Entities[d.EntityIndex];
				const float maxScale = MAX(MAX(e.Scale.x, e.Scale.y), e.Scale.z);

				BoundingSphere bs;
				bs.Center = e.Position + e.Scale * d.BoundingVolume.Center;
				bs.Radius = d.BoundingVolume.Radius * maxScale;

				DebugGeometry dg{};
				dg.Type = DebugGeometryType::SPHERE;
				dg.Color = Float4(Random::UNorm(i), Random::UNorm(i + 1), Random::UNorm(i + 2), 0.2f);
				dg.Position = bs.Center;
				dg.Scale = { bs.Radius, bs.Radius, bs.Radius };
				m_DebugGeometries.push_back(dg);
			}
		}
		
	}

	DrawDebugGeometries(context);

	if(DebugToolsConfig.LightHeatmap)
	{
		PipelineState pso = GFX::DefaultPipelineState();
		pso.BS.RenderTarget[0].BlendEnable = true;
		pso.BS.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		pso.BS.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
		pso.BS.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		pso.BS.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		pso.BS.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		pso.BS.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		GFX::Cmd::SetPipelineState(context, pso);

		GFX::Cmd::BindShader(context, m_LightHeatmapShader);
		GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.SceneInfoBuffer, 0);
		GFX::Cmd::BindSRV<PS>(context, m_VisibleLightsBuffer, 0);
		GFX::Cmd::BindVertexBuffer(context, Device::Get()->GetQuadBuffer());
		context->Draw(6, 0);
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
	UpdatePresentResources(context);
	UpdateCullingResources(context);
}

void ForwardPlus::UpdatePresentResources(ID3D11DeviceContext* context)
{
	if (m_FinalRT.Valid())
	{
		GFX::Storage::Free(m_FinalRT);
		GFX::Storage::Free(m_FinalRTSRV);
		GFX::Storage::Free(m_FinalRTHistory[0]);
		GFX::Storage::Free(m_FinalRTHistory[1]);
		GFX::Storage::Free(m_FinalRT_Depth);
		GFX::Storage::Free(m_MotionVectorRT);
	}

	m_FinalRT = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV );
	m_FinalRTSRV = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_SRV | RCF_CopyDest);
	m_FinalRTHistory[0] = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_SRV | RCF_CopyDest);
	m_FinalRTHistory[1] = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_SRV | RCF_CopyDest);
	m_FinalRT_Depth = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_DSV | RCF_Bind_SRV);
	m_MotionVectorRT = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R16G16_UNORM);

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
	struct TileCullingInfoCB
	{
		uint32_t ScreenSizeX;
		uint32_t ScreenSizeY;
		uint32_t NumTilesX;
		uint32_t NumTilesY;
	};

	if(m_VisibleLightsBuffer.Valid())
		GFX::Storage::Free(m_VisibleLightsBuffer);

	m_NumTilesX = MathUtility::CeilDiv(AppConfig.WindowWidth, TILE_SIZE);
	m_NumTilesY = MathUtility::CeilDiv(AppConfig.WindowHeight, TILE_SIZE);
	m_VisibleLightsBuffer = GFX::CreateBuffer(m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_SB | RCF_Bind_UAV);
}

void ForwardPlus::UpdateStats(ID3D11DeviceContext* context)
{
	GFX::Cmd::MarkerBegin(context, "Update stats");

	// Drawable stats
	RenderStats.TotalDrawables = 0;
	RenderStats.VisibleDrawables = 0;
	for (uint32_t i = 0; i < RenderGroupType::Count; i++)
	{
		RenderGroup& rg = MainSceneGraph.RenderGroups[i];
		RenderStats.TotalDrawables += rg.Drawables.GetSize();
		if (!DebugToolsConfig.FreezeGeometryCulling)
		{
			RenderStats.VisibleDrawables += rg.VisibilityMask.CountOnes();
		}
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
	GFX::Cmd::BindRenderTarget(context, m_FinalRT, m_FinalRT_Depth);
	GFX::Cmd::BindShader(context, m_DebugGeometryShader);
	GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 1);

	for (const DebugGeometry& dg : m_DebugGeometries)
	{
		BufferID vertexBuffer;

		// Prepare data
		{
			using namespace DirectX;

			XMMATRIX modelToWorld;

			switch (dg.Type)
			{
			case DebugGeometryType::CUBE:
				modelToWorld = XMMatrixAffineTransformation(dg.Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), dg.Position.ToXM());
				vertexBuffer = ForwardPlusPrivate::CubeVB;
				break;
			case DebugGeometryType::SPHERE:
				modelToWorld = XMMatrixAffineTransformation(dg.Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), dg.Position.ToXM());
				vertexBuffer = ForwardPlusPrivate::SphereVB;
				break;
			case DebugGeometryType::PLANE:
			{
				const Float3 upVec = Float3(0.0f, 1.0f, 0.0f);
				const Float3 normal = dg.Normal.NormalizeFast();
				const Float3 position = normal * dg.Distance;
				Float3 rotAxis = upVec.Cross(normal).Normalize();
				rotAxis = rotAxis.Length() < FLT_EPSILON ? upVec : rotAxis; // In case that upVec == normal
				const float rotAngle = acos(upVec.Dot(normal));
				modelToWorld = XMMatrixAffineTransformation(dg.Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM());
				modelToWorld = XMMatrixMultiply(modelToWorld, XMMatrixRotationAxis(rotAxis.ToXM(), rotAngle));
				modelToWorld = XMMatrixMultiply(modelToWorld, XMMatrixTranslation(position.x, position.y, position.z));
				vertexBuffer = ForwardPlusPrivate::PlaneVB;
			} break;
			default: NOT_IMPLEMENTED;
			}
			
			DebugGeometryDataCB debugGeometryDataCB{};
			debugGeometryDataCB.ModelToWorld = XMUtility::ToXMFloat4x4(XMMatrixTranspose(modelToWorld));
			debugGeometryDataCB.Color = dg.Color.ToXMF();
			GFX::Cmd::UploadToBuffer(context, m_DebugGeometryBuffer, 0, &debugGeometryDataCB, 0, sizeof(DebugGeometryDataCB));
		}

		// Draw
		{
			GFX::Cmd::BindVertexBuffer(context, vertexBuffer);
			GFX::Cmd::BindCBV<VS>(context, m_DebugGeometryBuffer, 0);
			GFX::Cmd::BindCBV<PS>(context, m_DebugGeometryBuffer, 0);
			context->Draw(GFX::GetNumElements(vertexBuffer), 0);
		}
	}

	GFX::Cmd::SetPipelineState(context, GFX::DefaultPipelineState());

	GFX::Cmd::MarkerEnd(context);

	m_DebugGeometries.clear();
}