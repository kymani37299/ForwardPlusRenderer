#pragma once

#include "Render/ResourceID.h"

struct ID3D11DeviceContext;

class PostprocessingRenderer
{
public:
	void Init(ID3D11DeviceContext* context);
	TextureID Process(ID3D11DeviceContext* context, TextureID colorInput, TextureID motionVectorInput);
	void ReloadTextureResources(ID3D11DeviceContext* context);

private:
	ShaderID m_PostprocessShader;

	TextureID m_TAAHistory[2]; // 0 - current frame, 1 - last frame
	TextureID m_PostprocessingResult;

	BufferID m_PostprocessingSettingsBuffer;
};