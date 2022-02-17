#include "RenderPasses.h"

#include "Common.h"

#include "Core/SceneGraph.h"
#include "Render/Commands.h"
#include "Render/Texture.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
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
}

void ScenePrepareRenderPass::OnDraw(ID3D11DeviceContext* context)
{
	MainSceneGraph.MainCamera.UpdateBuffer(context);
}

///////////////////////////////////////////////////
///////			Skybox					//////////
/////////////////////////////////////////////////

void SkyboxRenderPass::OnInit(ID3D11DeviceContext* context)
{
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

	m_Shader = GFX::CreateShader("Source/Shaders/skybox.hlsl");
	m_SkyboxPanorama = GFX::LoadTextureHDR("Resources/skybox_panorama.hdr", RCF_Bind_SRV);
	m_CubeVB = GFX::CreateVertexBuffer<Float3>(numVertices, (Float3*) vbData);
}

void SkyboxRenderPass::OnDraw(ID3D11DeviceContext* context)
{
	GFX::Cmd::BindShader(context, m_Shader);
	GFX::Cmd::BindVertexBuffer(context, m_CubeVB);
	GFX::Cmd::BindCBV<VS>(context, MainSceneGraph.MainCamera.CameraBuffer, 0);
	GFX::Cmd::BindSRV<PS>(context, m_SkyboxPanorama, 0);
	context->Draw(GFX::GetNumElements(m_CubeVB), 0);
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
	context->PSSetSamplers(0, GFX::GetStaticSamplersNum(), GFX::GetStaticSamplers());

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