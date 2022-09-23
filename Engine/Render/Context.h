#pragma once

#include <unordered_map>

#include "Render/RenderAPI.h"
#include "Render/Memory.h"
#include "System/ApplicationConfiguration.h"

struct Resource;
struct Buffer;
struct Texture;

struct Fence
{
	ComPtr<ID3D12Fence> Handle;
	uint64_t Value;
};

struct MemoryContext
{
	GPUAllocStrategy::Page SRVPersistentPage;
	DescriptorHeapGPU SRVHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 , 1};
};

struct BindTable
{
	std::vector<Resource*> CBVs;
	std::vector<Resource*> SRVs;
	std::vector<Resource*> UAVs;
};

struct GraphicsState
{
	static constexpr uint32_t PUSH_CONSTANT_BINDING = 128;

	BindTable Table;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC Pipeline;
	D3D12_SHADER_BYTECODE Compute;
	std::vector<Buffer*> VertexBuffers;
	Buffer* IndexBuffer = nullptr;
	Texture* RenderTarget = nullptr;
	Texture* DepthStencil = nullptr;
	uint32_t StencilRef = 0x00;
	D3D12_VIEWPORT Viewport;
	D3D12_RECT Scissor;
	D3D12_PRIMITIVE_TOPOLOGY Primitives;
	std::vector<D3D12_STATIC_SAMPLER_DESC> Samplers;
	std::vector<uint32_t> PushConstants;
	D3D12_COMMAND_SIGNATURE_DESC CommandSignature;

	GraphicsState();
};

// Not using: Table, Pipeline, PushConstants, Samplers
struct BoundGraphicsState : public GraphicsState
{
	bool Valid = false;
	uint32_t PSOHash = 0;
};

struct GraphicsContext
{
	ComPtr<ID3D12CommandQueue> CmdQueue;
	ComPtr<ID3D12CommandAllocator> CmdAlloc;
	ComPtr<ID3D12GraphicsCommandList> CmdList;
	MemoryContext MemContext;
	Fence CmdFence;

	BoundGraphicsState BoundState;
};

void ReleaseContextCache();
ID3D12CommandSignature* ApplyGraphicsState(GraphicsContext& context, const GraphicsState& state);