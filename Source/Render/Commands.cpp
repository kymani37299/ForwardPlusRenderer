#include "Commands.h"

#include "System/ApplicationConfiguration.h"
#include "Render/Resource.h"

namespace GFX
{
	namespace Cmd
	{
		void ResetContext(ID3D11DeviceContext1* context)
		{
			context->ClearState();

			const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)AppConfig.WindowWidth, (float)AppConfig.WindowHeight, 0.0f, 1.0f };
			context->RSSetViewports(1, &viewport);
			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		void BindShader(ID3D11DeviceContext1* context, ShaderID shaderID)
		{
			const Shader& shader = GFX::Storage::GetShader(shaderID);
			context->VSSetShader(shader.VS.Get(), nullptr, 0);
			context->PSSetShader(shader.PS.Get(), nullptr, 0);
			context->IASetInputLayout(shader.IL.Get());
		}

		void BindVertexBuffer(ID3D11DeviceContext1* context, BufferID bufferID)
		{
			const Buffer& buffer = GFX::Storage::GetBuffer(bufferID);

			uint32_t offsets = 0;
			uint32_t strides = buffer.ElementStride;
			ID3D11Buffer* dxBuffer = buffer.Handle.Get();

			context->IASetVertexBuffers(0, 1, &dxBuffer, &strides, &offsets);
		}

		void BindRenderTarget(ID3D11DeviceContext1* context, RenderTargetID rtID)
		{
			const Texture& colorTexture = GFX::Storage::GetTexture(rtID.ColorTexture);
			ID3D11DepthStencilView* dsv = nullptr;
			if (rtID.DepthTexture != static_cast<TextureID>(-1))
			{
				const Texture& depthTexture = GFX::Storage::GetTexture(rtID.DepthTexture);
				dsv = depthTexture.DSV.Get();
			}
			context->OMSetRenderTargets(1, colorTexture.RTV.GetAddressOf(), dsv);
		}

		void BindTextureSRV(ID3D11DeviceContext1* context, TextureID textureID, uint32_t slot)
		{
			const Texture& colorTex = GFX::Storage::GetTexture(textureID);
			ID3D11ShaderResourceView* srv = colorTex.SRV.Get();
			context->PSSetShaderResources(slot, 1, &srv);
		}

		void SetPipelineState(ID3D11DeviceContext1* context, const PipelineState& pipelineState)
		{
			const CompiledPipelineState& compiledState = GFX::CompilePipelineState(pipelineState);
			SetPipelineState(context, compiledState);
		}

		void SetPipelineState(ID3D11DeviceContext1* context, const CompiledPipelineState& pipelineState)
		{
			const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

			context->RSSetState(pipelineState.RS.Get());
			context->OMSetDepthStencilState(pipelineState.DS.Get(), 0xff);
			context->OMSetBlendState(pipelineState.BS.Get(), blendFactor, 0xff);
		}
	}
}