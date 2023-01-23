#pragma once

#include <vector>

#include "Render/Device.h"
#include "Render/Context.h"

struct D3D12_SUBRESOURCE_DATA;
struct Resource;
struct Shader;
struct Texture;
class ReadbackBuffer;
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

	// Delete on GPU timeline
	inline void Delete(GraphicsContext& context, const ComPtr<IUnknown>& resource) { context.MemContext.FrameDXResources.push_back(resource); }
	inline void Delete(GraphicsContext& context, const DescriptorAllocation& resource) { context.MemContext.FrameDescriptors.push_back(resource); }
	inline void Delete(GraphicsContext& context, Shader* resource) { context.MemContext.FrameShaders.push_back(resource); }
	inline void Delete(GraphicsContext& context, Resource* resource) { context.MemContext.FrameResources.push_back(resource); }
	
	void AddResourceTransition(std::vector<D3D12_RESOURCE_BARRIER>& barriers, Resource* resource, D3D12_RESOURCE_STATES wantedState);
	void TransitionResource(GraphicsContext& context, Resource* resource, D3D12_RESOURCE_STATES wantedState);

	void SetPushConstants(uint32_t shaderStages, GraphicsContext& context, const BindVector<uint32_t> values);

	void ClearRenderTarget(GraphicsContext& context, Texture* renderTarget);
	void ClearDepthStencil(GraphicsContext& context, Texture* depthStencil);

	void UploadToBufferImmediate(Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, uint32_t dataSize);
	void UploadToBuffer(GraphicsContext& context, Buffer* buffer, uint32_t dstOffset, const void* data, uint32_t srcOffset, uint32_t dataSize);
	void UploadToTexture(GraphicsContext& context, const void* data, Texture* texture, uint32_t mipIndex = 0, uint32_t arrayIndex = 0);
	void CopyToTexture(GraphicsContext& context, Texture* srcTexture, Texture* dstTexture, uint32_t mipIndex = 0);
	void CopyToBuffer(GraphicsContext& context, Buffer* srcBuffer, uint32_t srcOffset, Buffer* dstBuffer, uint32_t dstOffset, uint32_t size);

	void ClearBuffer(GraphicsContext& context, Buffer* buffer);

	void DrawFC(GraphicsContext& context, GraphicsState& state);

	void GenerateMips(GraphicsContext& context, Texture* texture);
	void ResolveTexture(GraphicsContext& context, Texture* inputTexture, Texture* outputTexture);

	void AddReadbackRequest(GraphicsContext& context, ReadbackBuffer* readbackBuffer);

	// Bindless table can only be used with context it is created
	BindlessTable CreateBindlessTable(GraphicsContext& context, std::vector<Texture*> textures, uint32_t registerSpace);
	BindlessTable CreateBindlessTable(GraphicsContext& context, std::vector<Buffer*> buffers, uint32_t registerSpace);
	void ReleaseBindlessTable(GraphicsContext& context, const BindlessTable& table);
}