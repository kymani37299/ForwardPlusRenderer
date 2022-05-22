#pragma once

#include "Render/ResourceID.h"

struct ID3D11DeviceContext;

class DebugRenderer
{
	enum class DebugGeometryType
	{
		CUBE,
		SPHERE
	};

	struct DebugGeometry
	{
		DebugGeometryType Type;
		Float3 Scale;
		Float4 Color;
		Float3 Position;
	};

public:
	void Init(ID3D11DeviceContext* context);
	void Draw(ID3D11DeviceContext* context, TextureID colorTarget, TextureID depthTarget, BufferID visibleLights);

	void DrawCube(Float3 position, Float4 color, Float3 scale)
	{
		DebugGeometry dg{};
		dg.Type = DebugGeometryType::CUBE;
		dg.Scale = scale;
		dg.Color = color;
		dg.Position = position;
		m_GeometriesToRender.push_back(dg);
	}

	void DrawSphere(Float3 position, Float4 color, Float3 scale)
	{
		DebugGeometry dg{};
		dg.Type = DebugGeometryType::SPHERE;
		dg.Scale = scale;
		dg.Color = color;
		dg.Position = position;
		m_GeometriesToRender.push_back(dg);
	}

private:
	void UpdateStats(ID3D11DeviceContext* context);
	void DrawGeometries(ID3D11DeviceContext* context);

private:
	std::vector<DebugGeometry> m_GeometriesToRender;

	ShaderID m_LightStatsShader;
	ShaderID m_DebugGeometryShader;
	ShaderID m_LightHeatmapShader;

	BufferID m_LightStatsBuffer;
	BufferID m_DebugGeometryBuffer;

	BufferID m_CubeVB;
	BufferID m_SphereVB;
};