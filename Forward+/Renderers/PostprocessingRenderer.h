#pragma once

#include <Engine/Render/ResourceID.h>

struct ID3D11DeviceContext;

class PostprocessingRenderer
{
public:
	void Init(ID3D11DeviceContext* context);
	TextureID Process(ID3D11DeviceContext* context, TextureID colorInput, TextureID motionVectorInput);
	void ReloadTextureResources(ID3D11DeviceContext* context);

private:
	void Step() { m_PostprocessRTIndex = (m_PostprocessRTIndex + 1) % 2; }
	TextureID GetInputTexture() const { return m_PostprocessRT[(m_PostprocessRTIndex + 1) % 2]; }
	TextureID GetOutputTexture() const { return m_PostprocessRT[m_PostprocessRTIndex]; }

private:
	ShaderID m_PostprocessShader;
	ShaderID m_BloomShader;

	// TAA
	TextureID m_TAAHistory[2]; // 0 - current frame, 1 - last frame

	// Bloom
	static constexpr uint32_t BLOOM_NUM_SAMPLES = 5;
	TextureID m_BloomTexturesDownsample[5];
	TextureID m_BloomTexturesUpsample[5];

	uint32_t m_PostprocessRTIndex = 0;
	TextureID m_PostprocessRT[2];
	TextureID m_ResolvedColor;
};