#include "DebugRenderer.h"

#include <Engine/Render/Commands.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Device.h>
#include <Engine/Utility/Random.h>

#include "Globals.h"
#include "Renderers/Util/ConstantManager.h"
#include "Scene/SceneGraph.h"

static BufferID GenerateSphereVB()
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
		vertices.push_back(verticesRaw[verticesRaw.size() - 1]);
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

void DebugRenderer::Init(ID3D11DeviceContext* context)
{
	m_CubeVB = GenerateCubeVB();
	m_SphereVB = GenerateSphereVB();

	m_DebugGeometryShader = GFX::CreateShader("Forward+/Shaders/debug_geometry.hlsl");
	m_LightHeatmapShader = GFX::CreateShader("Forward+/Shaders/light_heatmap.hlsl");
}

void DebugRenderer::Draw(ID3D11DeviceContext* context, TextureID colorTarget, TextureID depthTarget, BufferID visibleLights)
{
	UpdateStats(context);

	if (DebugToolsConfig.DrawBoundingSpheres)
	{
		for (uint32_t rgType = 0; rgType < EnumToInt(RenderGroupType::Count); rgType++)
		{
			RenderGroup& rg = MainSceneGraph.RenderGroups[rgType];
			const uint32_t numDrawables = rg.Drawables.GetSize();
			for (uint32_t i = 0; i < numDrawables; i++)
			{
				if (!rg.VisibilityMask.Get(i)) continue;

				const Drawable& d = rg.Drawables[i];
				const Entity& e = MainSceneGraph.Entities[d.EntityIndex];
				const float maxScale = MAX(MAX(e.Scale.x, e.Scale.y), e.Scale.z);

				BoundingSphere bs = e.GetBoundingVolume(d.BoundingVolume);

				DrawSphere(bs.Center, Float4(Random::UNorm(i), Random::UNorm(i + 1), Random::UNorm(i + 2), 0.2f), { bs.Radius, bs.Radius, bs.Radius });
			}
		}
	}

	if (DebugToolsConfig.DrawLightSpheres)
	{
		const uint32_t numLights = MainSceneGraph.Lights.GetSize();
		for (uint32_t i = 0; i < numLights; i++)
		{
			const Light& l = MainSceneGraph.Lights[i];
			DrawSphere(l.Position, Float4(l.Radiance.x, l.Radiance.y, l.Radiance.z, 0.2f), { l.Falloff.y, l.Falloff.y, l.Falloff.y });
		}
	}

	GFX::Cmd::BindRenderTarget(context, colorTarget, depthTarget);
	DrawGeometries(context);

	if (DebugToolsConfig.LightHeatmap && !DebugToolsConfig.DisableLightCulling && !DebugToolsConfig.FreezeGeometryCulling)
	{
		CBManager.Clear();
		CBManager.Add(MainSceneGraph.SceneInfoData, true);

		PipelineState pso = GFX::DefaultPipelineState();
		pso.BS.RenderTarget[0].BlendEnable = true;
		pso.BS.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		pso.BS.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
		pso.BS.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		pso.BS.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		pso.BS.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		pso.BS.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		GFX::Cmd::SetPipelineState(context, pso);

		GFX::Cmd::BindRenderTarget(context, colorTarget);
		GFX::Cmd::BindShader<VS | PS>(context, m_LightHeatmapShader);
		GFX::Cmd::BindSRV<PS>(context, visibleLights, 0);
		GFX::Cmd::DrawFC(context);
	}
}

void DebugRenderer::UpdateStats(ID3D11DeviceContext* context)
{
	GFX::Cmd::MarkerBegin(context, "Update stats");

	// Drawable stats
	RenderStats.TotalDrawables = 0;
	RenderStats.VisibleDrawables = 0;
	for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
	{
		RenderGroup& rg = MainSceneGraph.RenderGroups[i];
		RenderStats.TotalDrawables += rg.Drawables.GetSize();
		if (!DebugToolsConfig.FreezeGeometryCulling)
		{
			RenderStats.VisibleDrawables += rg.VisibilityMask.CountOnes();
		}
	}

	GFX::Cmd::MarkerEnd(context);
}

void DebugRenderer::DrawGeometries(ID3D11DeviceContext* context)
{
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
	GFX::Cmd::BindShader<PS|VS>(context, m_DebugGeometryShader);

	for (const DebugGeometry& dg : m_GeometriesToRender)
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
				vertexBuffer = m_CubeVB;
				break;
			case DebugGeometryType::SPHERE:
				modelToWorld = XMMatrixAffineTransformation(dg.Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), dg.Position.ToXM());
				vertexBuffer = m_SphereVB;
				break;
			default: NOT_IMPLEMENTED;
			}

			CBManager.Clear();
			CBManager.Add(MainSceneGraph.MainCamera.CameraData);
			CBManager.Add(XMUtility::ToHLSLFloat4x4(modelToWorld));
			CBManager.Add(dg.Color.ToXMF(), true);
		}

		// Draw
		{
			GFX::Cmd::BindVertexBuffer(context, vertexBuffer);
			context->Draw(GFX::GetNumElements(vertexBuffer), 0);
		}
	}

	GFX::Cmd::SetPipelineState(context, GFX::DefaultPipelineState());

	GFX::Cmd::MarkerEnd(context);

	m_GeometriesToRender.clear();
}
