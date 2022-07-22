#pragma once

#include <Engine/Common.h>

struct GraphicsContext;
struct GraphicsState;
struct Texture;
struct Buffer;
struct Shader;

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
	void Init(GraphicsContext& context);
	void Draw(GraphicsContext& context, Texture* colorTarget, Texture* depthTarget, Buffer* visibleLights);

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
	void UpdateStats(GraphicsContext& context);
	void DrawGeometries(GraphicsContext& context, GraphicsState& state);

private:
	std::vector<DebugGeometry> m_GeometriesToRender;

	ScopedRef<Shader> m_DebugGeometryShader;
	ScopedRef<Shader> m_LightHeatmapShader;

	ScopedRef<Buffer> m_CubeVB;
	ScopedRef<Buffer> m_SphereVB;
};