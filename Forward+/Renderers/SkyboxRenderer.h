#pragma once

#include <Engine/Common.h>

struct GraphicsContext;
struct GraphicsState;
struct Texture;
struct Buffer;
struct Shader;

class SkyboxRenderer
{
public:
	static constexpr uint32_t CUBEMAP_SIZE = 512;

	SkyboxRenderer(const std::string& texturePath): m_SkyboxTexturePath(texturePath) {}

	void Init(GraphicsContext& context);
	void Draw(GraphicsContext& context, GraphicsState& state);

	void OnShaderReload(GraphicsContext& context);

	Texture* GetIrradianceMap() const { return m_IrradianceCubemap.get(); }

private:
	std::string m_SkyboxTexturePath;

	ScopedRef<Buffer> m_CubeVB;
	ScopedRef<Shader> m_SkyboxShader;
	ScopedRef<Texture> m_SkyboxCubemap;
	ScopedRef<Texture> m_IrradianceCubemap;
};