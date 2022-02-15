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

void ScenePrepareRenderPass::OnInit(ID3D11DeviceContext1* context)
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

	Entity sponza = SceneLoading::LoadEntity("Resources/sponza/sponza.gltf");
	sponza.Scale *= 0.1f;
	MainSceneGraph.Entities.push_back(std::move(sponza));
	//MainSceneGraph.Entities.push_back(SceneLoading::LoadEntity("Resources/cube/cube.gltf"));
	MainSceneGraph.UpdateRenderData(context);
}

///////////////////////////////////////////////////
///////			Geometry				//////////
/////////////////////////////////////////////////

void GeometryRenderPass::OnInit(ID3D11DeviceContext1* context)
{
	m_Shader = GFX::CreateShader("Source/Shaders/geometry.hlsl");
}

void GeometryRenderPass::OnDraw(ID3D11DeviceContext1* context)
{
	MainSceneGraph.MainCamera.UpdateBuffer(context);

	PipelineState pso = GFX::DefaultPipelineState();
	pso.DS.DepthEnable = true;

	GFX::Cmd::SetPipelineState(context, pso);
	GFX::Cmd::BindShader(context, m_Shader, true);
	context->PSSetSamplers(0, GFX::GetStaticSamplersNum(), GFX::GetStaticSamplers());

	{
		ID3D11Buffer* cbv = GFX::DX_GetBuffer(MainSceneGraph.MainCamera.CameraBuffer);
		context->VSSetConstantBuffers(0, 1, &cbv);
		context->PSSetConstantBuffers(0, 1, &cbv);
	}

	{
		ID3D11ShaderResourceView* srv = GFX::DX_GetBufferSRV(MainSceneGraph.LightsBuffer);
		context->PSSetShaderResources(1, 1, &srv);
	}

	for (Entity e : MainSceneGraph.Entities)
	{
		{
			ID3D11Buffer* cbv = GFX::DX_GetBuffer(e.EntityBuffer);
			context->VSSetConstantBuffers(1, 1, &cbv);
		}

		for (Drawable d : e.Drawables)
		{
			Mesh& m = d.Mesh;
			GFX::Cmd::BindVertexBuffers(context, { m.Position, m.UV, m.Normal, m.Tangent });
			GFX::Cmd::BindIndexBuffer(context, m.Indices);

			ID3D11ShaderResourceView* srv = GFX::DX_GetTextureSRV(d.Material.Albedo);
			context->PSSetShaderResources(0, 1, &srv);

			context->DrawIndexed(GFX::GetNumBufferElements(m.Indices), 0, 0);
		}
	}
}