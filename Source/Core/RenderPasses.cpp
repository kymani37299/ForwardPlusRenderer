#include "RenderPasses.h"

#include "Common.h"

#include "Core/SceneGraph.h"
#include "Render/Commands.h"
#include "Render/Texture.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/Resource.h"
#include "Render/Device.h"
#include "Loading/SceneLoading.h"
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

///////////////////////////////////////////////////
///////			StaticResources			//////////
/////////////////////////////////////////////////
namespace StaticResources
{
	BufferID CubeVB;
}


///////////////////////////////////////////////////
///////			ScenePrepare			//////////
/////////////////////////////////////////////////

void ScenePrepareRenderPass::OnInit(ID3D11DeviceContext* context)
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

	MainSceneGraph.Entities.resize(2);
	MainSceneGraph.Entities[0].Scale *= 0.1f;

	SceneLoading::LoadEntityInBackground("Resources/sponza/sponza.gltf", MainSceneGraph.Entities[0]);
	SceneLoading::LoadEntity("Resources/cube/cube.gltf", MainSceneGraph.Entities[1]);
	
	MainSceneGraph.UpdateRenderData(context);

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

	StaticResources::CubeVB = GFX::CreateVertexBuffer<Float3>(numVertices, (Float3*)vbData);
}

void ScenePrepareRenderPass::OnDraw(ID3D11DeviceContext* context)
{
	MainSceneGraph.MainCamera.UpdateBuffer(context);
}

///////////////////////////////////////////////////
///////			Skybox					//////////
/////////////////////////////////////////////////

namespace
{
	TextureID PanoramaToCubemap(ID3D11DeviceContext* context, TextureID panoramaTexture, uint32_t cubemapSize)
	{
		const auto getCameraForFace = [](uint32_t faceIndex)
		{
			using namespace DirectX;
			static Float3 forwards[6] = { Float3(1.0f,  0.0f,  0.0f), Float3(-1.0f,  0.0f,  0.0f), Float3(0.0f,  -1.0f,  0.0f), Float3(0.0f, 1.0f,  0.0f), Float3(0.0f,  0.0f,  1.0f), Float3(0.0f,  0.0f, -1.0f) };
			//static Float3 ups[6] = { Float3(0.0f, -1.0f,  0.0f), Float3(0.0f, -1.0f,  0.0f), Float3(0.0f,  0.0f,  -1.0f), Float3(0.0f,  0.0f, 1.0f), Float3(0.0f, -1.0f,  0.0f), Float3(0.0f, -1.0f,  0.0f) };
			//
			//XMMATRIX viewMat = XMMatrixLookAtLH(Float3(0.0f, 0.0f, 0.0f).ToXM(), forwards[faceIndex].ToXM(), ups[faceIndex].ToXM());
			
			XMMATRIX viewMat = XMMatrixLookAtLH(Float3(0.0f, 0.0f, 0.0f).ToXM(), forwards[faceIndex].ToXM(), Float3(0.0f, 1.0f, 0.0f).ToXM());
			viewMat = XMMatrixTranspose(viewMat);

			XMMATRIX projMat = XMMatrixPerspectiveFovLH(3.1415f / 2.0f, 1.0f, 0.1f, 10.0f);
			projMat = XMMatrixTranspose(projMat);

			XMMATRIX viewProj = XMMatrixMultiply(viewMat, projMat);
			XMFLOAT4X4 ret;
			XMStoreFloat4x4(&ret, viewProj);
			return ret;
		};

		TextureID cubemapTex = GFX::CreateTextureArray(cubemapSize, cubemapSize, 6, RCF_Bind_SRV | RCF_Bind_RTV | RCF_Cubemap);
		const Texture& cubemapTexHandle = GFX::Storage::GetTexture(cubemapTex);
		BufferID cameraCB = GFX::CreateConstantBuffer<DirectX::XMFLOAT4X4>();
		ShaderID shader = GFX::CreateShader("Source/Shaders/quadrilateral2cubemap.hlsl");

		GFX::Cmd::MarkerBegin(context, "PanoramaToCubemap");

		GFX::Cmd::BindVertexBuffer(context, StaticResources::CubeVB);
		GFX::Cmd::BindShader(context, shader);
		GFX::Cmd::BindCBV<VS>(context, cameraCB, 0);
		GFX::Cmd::BindSRV<PS>(context, panoramaTexture, 0);
		GFX::Cmd::SetupStaticSamplers<PS>(context);

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
			GFX::Cmd::UploadToBuffer(context, cameraCB, &worldToClip, sizeof(DirectX::XMFLOAT4X4));

			context->OMSetRenderTargets(1, &rtv, nullptr);
			context->Draw(GFX::GetNumElements(StaticResources::CubeVB), 0);

			rtv->Release();
		}

		GFX::Storage::Free(shader);
		GFX::Storage::Free(cameraCB);

		GFX::Cmd::MarkerEnd(context);

		return cubemapTex;
	}
}

void SkyboxRenderPass::OnInit(ID3D11DeviceContext* context)
{
	m_Shader = GFX::CreateShader("Source/Shaders/skybox.hlsl");
	OnShaderReload(context);
}

void SkyboxRenderPass::OnDraw(ID3D11DeviceContext* context)
{
	GFX::Cmd::BindShader(context, m_Shader);
	GFX::Cmd::BindVertexBuffer(context, StaticResources::CubeVB);
	GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
	GFX::Cmd::BindSRV<PS>(context, m_SkyboxCubemap, 0);
	GFX::Cmd::SetupStaticSamplers<PS>(context);
	context->Draw(GFX::GetNumElements(StaticResources::CubeVB), 0);
}

void SkyboxRenderPass::OnShaderReload(ID3D11DeviceContext* context)
{
	if (m_SkyboxCubemap.Valid())
	{
		GFX::Storage::Free(m_SkyboxCubemap);
	}
	TextureID skyboxPanorama = GFX::LoadTextureHDR("Resources/skybox_panorama.hdr", RCF_Bind_SRV);
	m_SkyboxCubemap = PanoramaToCubemap(context, skyboxPanorama, 512);
	GFX::Storage::Free(skyboxPanorama);
}

///////////////////////////////////////////////////
///////			DepthPrepass			//////////
/////////////////////////////////////////////////

void DepthPrepassRenderPass::OnInit(ID3D11DeviceContext* context)
{
	m_Shader = GFX::CreateShader("Source/Shaders/depth.hlsl", {}, SCF_VS);
}

void DepthPrepassRenderPass::OnDraw(ID3D11DeviceContext* context)
{
	PipelineState pso = GFX::DefaultPipelineState();
	pso.DS.DepthEnable = true;

	GFX::Cmd::SetPipelineState(context, pso);
	GFX::Cmd::BindShader(context, m_Shader, true);
	GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
	GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);

	for (Entity& e : MainSceneGraph.Entities)
	{
		GFX::Cmd::BindCBV<VS>(context, e.EntityBuffer, 1);

		const auto func = [&context](const Drawable& d) {
			if (!d.Material.UseAlphaDiscard && !d.Material.UseBlend)
			{
				const Mesh& m = d.Mesh;
				GFX::Cmd::BindVertexBuffers(context, { m.Position, m.UV, m.Normal, m.Tangent });
				GFX::Cmd::BindIndexBuffer(context, m.Indices);
				context->DrawIndexed(GFX::GetNumElements(m.Indices), 0, 0);
			}

		};
		e.Drawables.ForEach(func);
	}
}

///////////////////////////////////////////////////
///////			Geometry				//////////
/////////////////////////////////////////////////

void GeometryRenderPass::OnInit(ID3D11DeviceContext* context)
{
	m_Shader = GFX::CreateShader("Source/Shaders/geometry.hlsl");
	m_AlphaDiscardShader = GFX::CreateShader("Source/Shaders/geometry.hlsl", { "ALPHA_DISCARD" });
}

void GeometryRenderPass::OnDraw(ID3D11DeviceContext* context)
{
	{
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;
		pso.DS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		GFX::Cmd::SetPipelineState(context, pso);
	}

	GFX::Cmd::BindShader(context, m_Shader, true);
	GFX::Cmd::SetupStaticSamplers<PS>(context);
	GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
	GFX::Cmd::BindCBV<PS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
	GFX::Cmd::BindSRV<PS>(context, MainSceneGraph.LightsBuffer, 3);

	const auto drawFunc = [&context](const Drawable& d)
	{
		GFX::Cmd::BindCBV<PS>(context, d.Material.MaterialParams, 2);

		const Mesh& m = d.Mesh;
		GFX::Cmd::BindVertexBuffers(context, { m.Position, m.UV, m.Normal, m.Tangent });
		GFX::Cmd::BindIndexBuffer(context, m.Indices);

		ID3D11ShaderResourceView* srv[] = { GFX::DX_SRV(d.Material.Albedo), GFX::DX_SRV(d.Material.MetallicRoughness), GFX::DX_SRV(d.Material.Normal) };
		context->PSSetShaderResources(0, 3, srv);

		context->DrawIndexed(GFX::GetNumElements(m.Indices), 0, 0);
	};

	for (Entity& e : MainSceneGraph.Entities)
	{
		GFX::Cmd::BindCBV<VS>(context, e.EntityBuffer, 1);
		const auto draw = [&context, &drawFunc](const Drawable& d) { if (!d.Material.UseAlphaDiscard && !d.Material.UseBlend) drawFunc(d); };
		e.Drawables.ForEach(draw);
	}

	{
		PipelineState pso = GFX::DefaultPipelineState();
		pso.DS.DepthEnable = true;
		GFX::Cmd::SetPipelineState(context, pso);
	}
	GFX::Cmd::BindShader(context, m_AlphaDiscardShader, true);

	for (Entity& e : MainSceneGraph.Entities)
	{
		GFX::Cmd::BindCBV<VS>(context, e.EntityBuffer, 1);
		const auto draw = [&context, &drawFunc](const Drawable& d) { if (d.Material.UseAlphaDiscard) drawFunc(d); };
		e.Drawables.ForEach(draw);
	}
}