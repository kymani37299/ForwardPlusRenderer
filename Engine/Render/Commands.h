#pragma once

#include <vector>

#include "Render/Device.h"
#include "Render/Context.h"

struct D3D12_SUBRESOURCE_DATA;
struct GraphicsContext;
struct GraphicsState;
struct Resource;
struct Shader;
struct Texture;
struct ID3D12CommandSignature;

enum D3D12_TEXTURE_ADDRESS_MODE;
enum D3D12_FILTER;

namespace GFX::Cmd
{
	GraphicsContext* CreateGraphicsContext();
	void FlushContext(GraphicsContext& context);
	void SubmitContext(GraphicsContext& context);
	void ResetContext(GraphicsContext& context);

	void MarkerBegin(GraphicsContext& context, const std::string& name);
	void MarkerEnd(GraphicsContext& context);

	void AddResourceTransition(std::vector<D3D12_RESOURCE_BARRIER>& barriers, Resource* resource, D3D12_RESOURCE_STATES wantedState);
	void TransitionResource(GraphicsContext& context, Resource* resource, D3D12_RESOURCE_STATES wantedState);

	ID3D12CommandSignature* BindState(GraphicsContext& context, const GraphicsState& state);
	void UpdatePushConstants(GraphicsContext& context, const GraphicsState& state);
	void ClearRenderTarget(GraphicsContext& context, Texture* renderTarget);
	void ClearDepthStencil(GraphicsContext& context, Texture* depthStencil);

	void UploadToBufferCPU(Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, size_t dataSize);
	void UploadToBufferGPU(GraphicsContext& context, Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, size_t dataSize);
	void UploadToBuffer(GraphicsContext& context, Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, size_t dataSize);
	void UploadToTexture(GraphicsContext& context, const void* data, Texture* texture, uint32_t mipIndex = 0, uint32_t arrayIndex = 0);
	void CopyToTexture(GraphicsContext& context, Texture* srcTexture, Texture* dstTexture, uint32_t mipIndex = 0);
	void CopyToBuffer(GraphicsContext& context, Buffer* srcBuffer, uint32_t srcOffset, Buffer* dstBuffer, uint32_t dstOffset, uint32_t size);

	void DrawFC(GraphicsContext& context, GraphicsState& state);

	void BindShader(GraphicsState& state, Shader* shader, uint8_t stages, std::vector<std::string> config = {}, bool multiInput = false);
	void BindRenderTarget(GraphicsState& state, Texture* renderTarget);
	void BindDepthStencil(GraphicsState& state, Texture* depthStencil);
	void BindSampler(GraphicsState& state, uint32_t slot, D3D12_TEXTURE_ADDRESS_MODE addressMode, D3D12_FILTER filter);

	void GenerateMips(GraphicsContext& context, Texture* texture);
	void ResolveTexture(GraphicsContext& context, Texture* inputTexture, Texture* outputTexture);
}