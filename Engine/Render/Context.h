#pragma once

#include <unordered_map>

#include "Render/RenderAPI.h"
#include "Render/Memory.h"
#include "Render/Shader.h"
#include "System/ApplicationConfiguration.h"

struct Resource;
struct Buffer;
struct Texture;
struct Shader;

struct Fence
{
	ComPtr<ID3D12Fence> Handle;
	uint64_t Value;
};

struct MemoryContext
{
	GPUAllocStrategy::Page SRVPersistentPage;
	DescriptorHeapGPU SRVHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1 , 1};
	DescriptorHeapGPU SMPHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1, 1 };
};

struct Sampler
{
	D3D12_FILTER Filter;
	D3D12_TEXTURE_ADDRESS_MODE AddressMode;
};

struct BindTable
{
	std::vector<Resource*> CBVs;
	std::vector<Resource*> SRVs;
	std::vector<Resource*> UAVs;
	std::vector<Sampler>   SMPs;
};

enum class RenderPrimitiveType
{
	PointList,

	LineList,
	LineListAdj,
	LineStrip,
	LineStripAdj,
	
	TriangleList,
	TriangleListAdj,
	TriangleStrip,
	TriangleStripAdj,

	PatchList,
};

struct GraphicsState
{
	// Bindings
	BindTable Table;
	Buffer* IndexBuffer = nullptr;
	std::vector<Buffer*> VertexBuffers;
	std::vector<Texture*> RenderTargets;
	Texture* DepthStencil = nullptr;
	uint32_t PushConstantBinding = 128;
	std::vector<uint32_t> PushConstants;

	// Shader
	Shader* Shader = nullptr;
	uint32_t ShaderStages = VS | PS;
	std::vector<std::string> ShaderConfig = {};

	// State
	D3D12_BLEND_DESC BlendState;
	D3D12_RASTERIZER_DESC RasterizerState;
	D3D12_DEPTH_STENCIL_DESC DepthStencilState;
	
	bool UseCustomViewport = false;
	D3D12_VIEWPORT CustomViewport;
	bool UseCustomScissor = false;
	D3D12_RECT CustomScissor;

	// Other
	uint32_t StencilRef = 0x00;
	RenderPrimitiveType PrimitiveType = RenderPrimitiveType::TriangleList;
	uint32_t NumControlPoints = 3;
	D3D12_COMMAND_SIGNATURE_DESC CommandSignature;

	GraphicsState();
};

// Not using: Table, Pipeline, PushConstants, Samplers
struct BoundGraphicsState
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