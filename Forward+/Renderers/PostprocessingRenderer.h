#pragma once

#include <Engine/Common.h>

struct GraphicsContext;
struct GraphicsState;
struct Texture;
struct Buffer;
struct Shader;

class PostprocessingRenderer
{
public:
	PostprocessingRenderer();
	~PostprocessingRenderer();

	void Init(GraphicsContext& context);
	Texture* Process(GraphicsContext& context, Texture* colorInput, Texture* motionVectorInput);
	void ReloadTextureResources(GraphicsContext& context);

private:
	void Step() { m_PostprocessRTIndex = (m_PostprocessRTIndex + 1) % 2; }
	Texture* GetInputTexture() const { return m_PostprocessRT[(m_PostprocessRTIndex + 1) % 2].get(); }
	Texture* GetOutputTexture() const { return m_PostprocessRT[m_PostprocessRTIndex].get(); }

private:
	ScopedRef<Shader> m_PostprocessShader;
	ScopedRef<Shader> m_BloomShader;

	// TAA
	ScopedRef<Texture> m_TAAHistory[2]; // 0 - current frame, 1 - last frame

	// Bloom
	static constexpr uint32_t BLOOM_NUM_SAMPLES = 5;
	ScopedRef<Texture> m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES];
	ScopedRef<Texture> m_BloomTexturesUpsample[BLOOM_NUM_SAMPLES];

	uint32_t m_PostprocessRTIndex = 0;
	ScopedRef<Texture> m_PostprocessRT[2];
	ScopedRef<Texture> m_ResolvedColor;
};