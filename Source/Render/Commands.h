#pragma once

#include "Render/RenderAPI.h"
#include "Render/Resource.h"

namespace GFX
{
	namespace Cmd
	{
		void ResetContext(ID3D11DeviceContext1* context);

		void BindShader(ID3D11DeviceContext1* context, ShaderID shaderID);
		void BindVertexBuffer(ID3D11DeviceContext1* context, BufferID bufferID);
		void BindRenderTarget(ID3D11DeviceContext1* context, RenderTargetID rtID);
		void BindTextureSRV(ID3D11DeviceContext1* context, TextureID textureID, uint32_t slot);
	}
}