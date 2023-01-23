#pragma once

#include <unordered_map>

#include "Render/RenderAPI.h"
#include "Render/DescriptorHeap.h"
#include "Render/Shader.h"
#include "Utility/MathUtility.h"
#include "System/ApplicationConfiguration.h"

struct Resource;
struct Buffer;
class ReadbackBuffer;
struct Texture;
struct TextureSubresource;
struct Shader;
struct GraphicsContext;

struct Fence
{
	ComPtr<ID3D12Fence> Handle;
	uint64_t Value;
};

struct MemoryContext
{
	// Pernament descriptors
	DescriptorHeap SRVHeap{ true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, 1 };
	DescriptorHeap SMPHeap{ true, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1, 1 };

	// Frame resources
	std::vector<ComPtr<IUnknown>> FrameDXResources;
	std::vector<DescriptorAllocation> FrameDescriptors;
	std::vector<Shader*> FrameShaders;
	std::vector<Resource*> FrameResources;
};

struct Sampler
{
	D3D12_FILTER Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	D3D12_TEXTURE_ADDRESS_MODE AddressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	Float4 BorderColor{ 0.0f, 0.0f, 0.0f, 0.0f };
};

template<typename T, size_t Size = 128>
class BindVector
{
public:
	BindVector() { m_Descriptors.resize(Size+1); }

	T* begin() { return &m_Descriptors[0]; }
	T* end() { return &m_Descriptors[m_DescriptorCount]; }
	const T* begin() const { return &m_Descriptors[0]; }
	const T* end() const { return &m_Descriptors[m_DescriptorCount]; }

	bool empty() const { return m_DescriptorCount == 0; }
	size_t size() const { return m_DescriptorCount; }
	const void* data() const { return m_Descriptors.data(); }

	const T& operator [] (size_t index) const { return m_Descriptors[index]; }

	T& operator [] (size_t index)
	{
		const size_t reqSize = index + 1;
		if (reqSize > m_DescriptorCount)
		{
			m_DescriptorCount = reqSize;
		}
		return m_Descriptors[index];
	}

private:
	size_t m_DescriptorCount = 0;
	std::vector<T> m_Descriptors;
};

struct BindTable
{
	BindVector<Resource*> CBVs;
	BindVector<Resource*> SRVs;
	BindVector<Resource*> UAVs;
	BindVector<Sampler>   SMPs;
};

struct BindlessTable
{
	uint32_t RegisterSpace = 1;
	uint32_t DescriptorCount = 0;
	DescriptorAllocation DescriptorTable;
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
	BindVector<Buffer*, 8> VertexBuffers;
	BindVector<Texture*, 8> RenderTargets;
	Texture* DepthStencil = nullptr;
	BindVector<BindlessTable> BindlessTables = {};
	uint32_t PushConstantBinding = 128;
	uint32_t PushConstantCount = 0;

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

class StagingResourcesContext
{
public:
	struct StagingTextureRequest
	{
		uint32_t Width;
		uint32_t Height;
		uint32_t NumMips;
		uint64_t CreationFlags;
		DXGI_FORMAT Format;
	};

	struct StagingTexture
	{
		Texture* TextureResource;
	};

	// Same texture can be used multiple times in frame since all is on GPU timeline
	StagingTexture* GetTransientTexture(const StagingTextureRequest& request);

	void ClearTransientTextures(GraphicsContext& context);

private:
	std::unordered_map<uint32_t, StagingTexture*> m_TransientStagingTextures;
};

struct BoundGraphicsState
{
	bool Valid = false;
	uint32_t PSOHash = 0;
};

struct GraphicsContext
{
	~GraphicsContext();
	ID3D12CommandSignature* ApplyState(const GraphicsState& state);

	bool Closed = false;
	ComPtr<ID3D12CommandQueue> CmdQueue;
	ComPtr<ID3D12CommandAllocator> CmdAlloc;
	ComPtr<ID3D12GraphicsCommandList> CmdList;
	MemoryContext MemContext;
	Fence CmdFence;

	std::vector<ReadbackBuffer*> PendingReadbacks;

	// Cache
	std::unordered_map<uint32_t, ComPtr<ID3D12RootSignature>> RootSignatureCache;
	std::unordered_map<uint32_t, ComPtr<ID3D12PipelineState>> PSOCache;
	std::unordered_map<uint32_t, DescriptorAllocation> SamplerCache;

	StagingResourcesContext StagingResources;

	BoundGraphicsState BoundState;
};