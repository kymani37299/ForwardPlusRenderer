#include "DebugRenderer.h"

#include <Engine/Render/Commands.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Device.h>
#include <Engine/Utility/Random.h>

#include "Globals.h"
#include "Renderers/Util/ConstantManager.h"
#include "Scene/SceneGraph.h"

struct DebugGeometrySB
{
	DirectX::XMFLOAT4X4 ModelToWorld;
	DirectX::XMFLOAT4 Color;
};

static Buffer* GenerateSphereVB(GraphicsContext& context)
{
	constexpr uint32_t parallels = 11;
	constexpr uint32_t meridians = 22;
	constexpr float PI = 3.14159265358979323846f;

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
			float azimuth = 2.0f * PI * float(i) / float(meridians);
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

	ResourceInitData initData{ &context , vertices.data() };
	return GFX::CreateVertexBuffer<Float3>((uint32_t) vertices.size(), &initData);
}

Buffer* GenerateCubeVB(GraphicsContext& context)
{
	static const float vbData[] = { -1.0f,-1.0f,-1.0f,-1.0f,-1.0f, 1.0f,-1.0f, 1.0f, 1.0f,1.0f, 1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f, 1.0f,-1.0f,1.0f,-1.0f, 1.0f,-1.0f,-1.0f,-1.0f,1.0f,-1.0f,-1.0f,1.0f, 1.0f,-1.0f,1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f, 1.0f, 1.0f,-1.0f, 1.0f,-1.0f,1.0f,-1.0f, 1.0f,-1.0f,-1.0f, 1.0f,-1.0f,-1.0f,-1.0f,-1.0f, 1.0f, 1.0f,-1.0f,-1.0f, 1.0f,1.0f,-1.0f, 1.0f,1.0f, 1.0f, 1.0f,1.0f,-1.0f,-1.0f,1.0f, 1.0f,-1.0f,1.0f,-1.0f,-1.0f,1.0f, 1.0f, 1.0f,1.0f,-1.0f, 1.0f,1.0f, 1.0f, 1.0f,1.0f, 1.0f,-1.0f,-1.0f, 1.0f,-1.0f,1.0f, 1.0f, 1.0f,-1.0f, 1.0f,-1.0f,-1.0f, 1.0f, 1.0f,1.0f, 1.0f, 1.0f,-1.0f, 1.0f, 1.0f,1.0f,-1.0f, 1.0f };
	static uint32_t numVertices = STATIC_ARRAY_SIZE(vbData) / 3;
	ResourceInitData initData{ &context , vbData };
	return GFX::CreateVertexBuffer<Float3>(numVertices, &initData);
}

void DebugRenderer::Init(GraphicsContext& context)
{
	m_CubeVB = ScopedRef<Buffer>(GenerateCubeVB(context));
	m_SphereVB = ScopedRef<Buffer>(GenerateSphereVB(context));

	m_DebugGeometryShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/debug_geometry.hlsl"));
	m_LightHeatmapShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/light_heatmap.hlsl"));

	m_DebugGeometriesBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(sizeof(DebugGeometrySB), sizeof(DebugGeometrySB), RCF_None));
}

void DebugRenderer::Draw(GraphicsContext& context, Texture* colorTarget, Texture* depthTarget, Buffer* visibleLights)
{
	if (DebugViz.BoundingSpheres)
	{
		for (uint32_t rgType = 0; rgType < EnumToInt(RenderGroupType::Count); rgType++)
		{
			RenderGroup& rg = MainSceneGraph->RenderGroups[rgType];
			const uint32_t numDrawables = rg.Drawables.GetSize();
			for (uint32_t i = 0; i < numDrawables; i++)
			{
				const Drawable& d = rg.Drawables[i];
				const BoundingSphere bs = d.GetBoundingVolume();
				const float fi = (float)i;
				DrawSphere(bs.Center, Float4(Random::UNorm(fi), Random::UNorm(fi + 1.0f), Random::UNorm(fi + 2.0f), 0.2f), { bs.Radius, bs.Radius, bs.Radius });
			}
		}
	}

	if (DebugViz.LightSpheres)
	{
		const uint32_t numLights = MainSceneGraph->Lights.GetSize();
		for (uint32_t i = 0; i < numLights; i++)
		{
			const Light& l = MainSceneGraph->Lights[i];
			DrawSphere(l.Position, Float4(l.Radiance.x, l.Radiance.y, l.Radiance.z, 0.2f), { l.Falloff.y, l.Falloff.y, l.Falloff.y });
		}
	}

	GraphicsState geometriesState;
	geometriesState.RenderTargets[0] = colorTarget;
	geometriesState.DepthStencil = depthTarget;
	DrawGeometries(context, geometriesState);

	if (DebugViz.LightHeatmap && RenderSettings.Culling.LightCullingEnabled)
	{
		CBManager.Clear();
		CBManager.Add(MainSceneGraph->SceneInfoData);

		GraphicsState heatmapState;
		heatmapState.Table.CBVs[0] = CBManager.GetBuffer();
		heatmapState.Table.SRVs[0] = visibleLights;
		heatmapState.RenderTargets[0] = colorTarget;
		heatmapState.Shader = m_LightHeatmapShader.get();
		heatmapState.BlendState.RenderTarget[0].BlendEnable = true;
		heatmapState.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		heatmapState.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
		heatmapState.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		heatmapState.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		heatmapState.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		heatmapState.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		GFX::Cmd::DrawFC(context, heatmapState);
	}
}

void DebugRenderer::DrawGeometries(GraphicsContext& context, GraphicsState& state)
{
	GFX::Cmd::MarkerBegin(context, "Debug Geometries");

	std::sort(m_GeometriesToRender.begin(), m_GeometriesToRender.end(), [](const DebugGeometry& a, const DebugGeometry& b)
		{
			const Float3 cameraPos = MainSceneGraph->MainCamera.CurrentTranform.Position;
			const float aDist = (cameraPos - a.Position).Abs().SumElements();
			const float bDist = (cameraPos - b.Position).Abs().SumElements();
			return aDist < bDist;
		});

	// Prepare pipeline
	state.DepthStencilState.DepthEnable = true;
	state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	state.BlendState.RenderTarget[0].BlendEnable = true;
	state.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	state.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
	state.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	state.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	state.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	state.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	state.Shader = m_DebugGeometryShader.get();

	Buffer* typeToVB[EnumToInt(DebugGeometryType::COUNT)] = { m_CubeVB.get(), m_SphereVB.get()};

	std::vector<DebugGeometrySB> debugGeometries{};
	for (uint32_t i = 0; i < EnumToInt(DebugGeometryType::COUNT); i++)
	{
		DebugGeometryType type = IntToEnum<DebugGeometryType>(i);
		for (const DebugGeometry& dg : m_GeometriesToRender)
		{
			if (dg.Type != type) continue;

			DebugGeometrySB geoEntry{};
			geoEntry.ModelToWorld = XMUtility::ToHLSLFloat4x4(DirectX::XMMatrixAffineTransformation(dg.Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), dg.Position.ToXM()));
			geoEntry.Color = dg.Color.ToXMF();
			debugGeometries.push_back(geoEntry);
		}

		if (debugGeometries.empty()) continue;

		if (debugGeometries.size() > m_DebugGeometriesBuffer->ByteSize / m_DebugGeometriesBuffer->Stride)
		{
			GFX::ResizeBuffer(context, m_DebugGeometriesBuffer.get(), (uint32_t) debugGeometries.size() * m_DebugGeometriesBuffer->Stride);
		}
		GFX::Cmd::UploadToBuffer(context, m_DebugGeometriesBuffer.get(), 0, debugGeometries.data(), 0, (uint32_t) debugGeometries.size() * m_DebugGeometriesBuffer->Stride);
		state.Table.SRVs[0] = m_DebugGeometriesBuffer.get();

		CBManager.Clear();
		CBManager.Add(MainSceneGraph->MainCamera.CameraData);
		state.Table.CBVs[0] = CBManager.GetBuffer();
		state.VertexBuffers[0] = typeToVB[i];
		context.ApplyState(state);
		context.CmdList->DrawInstanced(m_SphereVB->ByteSize / m_SphereVB->Stride, (uint32_t) debugGeometries.size(), 0, 0);
		debugGeometries.clear();
	}

	GFX::Cmd::MarkerEnd(context);

	m_GeometriesToRender.clear();
}
