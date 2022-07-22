#include "Context.h"

#include "Render/Device.h"
#include "Render/Commands.h"
#include "Render/Resource.h"
#include "Render/Texture.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Utility/Hash.h"

// TODO: APPLY TO TO MTR
// WARNING: Not multithread safe
static std::unordered_map<uint32_t, ComPtr<ID3D12RootSignature>> RootSignatureCache;
static std::unordered_map<uint32_t, ComPtr<ID3D12PipelineState>> PSOCache;

GraphicsState::GraphicsState()
{
	D3D12_BLEND_DESC blend;
	blend.AlphaToCoverageEnable = false;
	blend.IndependentBlendEnable = false;
	for (uint32_t i = 0; i < 8; i++)
	{
		blend.RenderTarget[i].BlendEnable = false;
		blend.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		blend.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blend.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		blend.RenderTarget[i].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blend.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		blend.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ONE;
		blend.RenderTarget[i].LogicOpEnable = false;
		blend.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
	}

	D3D12_RASTERIZER_DESC raster;
	raster.AntialiasedLineEnable = false;
	raster.DepthBias = 0;
	raster.DepthBiasClamp = 0.0f;
	raster.DepthClipEnable = true;
	raster.FrontCounterClockwise = false;
	raster.MultisampleEnable = false;
	raster.SlopeScaledDepthBias = false;
	raster.FillMode = D3D12_FILL_MODE_SOLID;
	raster.CullMode = D3D12_CULL_MODE_NONE;
	raster.ForcedSampleCount = 0;
	raster.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_DEPTH_STENCILOP_DESC stencilOp;
	stencilOp.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	stencilOp.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	stencilOp.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	stencilOp.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_DEPTH_STENCIL_DESC depthStencil;
	depthStencil.DepthEnable = false;
	depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencil.StencilEnable = false;
	depthStencil.StencilReadMask = 0xff;
	depthStencil.StencilWriteMask = 0xff;
	depthStencil.FrontFace = stencilOp;
	depthStencil.BackFace = stencilOp;

	Pipeline.pRootSignature = nullptr;
	Pipeline.VS = { nullptr, 0 };
	Pipeline.PS = { nullptr, 0 };
	Pipeline.DS = { nullptr, 0 };
	Pipeline.HS = { nullptr, 0 };
	Pipeline.GS = { nullptr, 0 };
	Pipeline.StreamOutput = { nullptr, 0, nullptr, 0, 0 };
	Pipeline.BlendState = blend;
	Pipeline.SampleMask = UINT_MAX;
	Pipeline.RasterizerState = raster;
	Pipeline.DepthStencilState = depthStencil;
	Pipeline.InputLayout = { nullptr, 0 };
	Pipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	Pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	Pipeline.NumRenderTargets = 1;
	Pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	for (uint32_t i = 1; i < 8; i++) Pipeline.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
	Pipeline.DSVFormat = DXGI_FORMAT_R24G8_TYPELESS;
	Pipeline.SampleDesc.Count = 1;
	Pipeline.SampleDesc.Quality = 0;
	Pipeline.NodeMask = 0;
	Pipeline.CachedPSO = { nullptr, 0 };
	Pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	Compute = { nullptr, 0 };
	Viewport = { 0.0f, 0.0f, (float)AppConfig.WindowWidth, (float)AppConfig.WindowHeight, 0.0f, 1.0f };
	Scissor = { 0,0, (long)AppConfig.WindowWidth, (long)AppConfig.WindowHeight };
	Primitives = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

static std::vector<D3D12_DESCRIPTOR_RANGE> CreateDescriptorRanges(std::vector<Resource*> bindings, D3D12_DESCRIPTOR_RANGE_TYPE rangeType)
{
	std::vector<D3D12_DESCRIPTOR_RANGE> ranges;

	for (uint32_t i = 0; i < bindings.size(); i++)
	{
		if (!bindings[i]) continue;

		D3D12_DESCRIPTOR_RANGE range;
		range.RangeType = rangeType;
		range.BaseShaderRegister = i;
		range.NumDescriptors = 1;
		range.RegisterSpace = 0;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		ranges.push_back(range);
	}
	return ranges;
}

namespace
{
	enum class BindingType
	{
		CBV,
		SRV,
		UAV,
	};
}

static void UpdateDescriptorData(const std::vector<Resource*>& bindings, DescriptorHeapGPU& heap, GPUAllocStrategy::Page& page, BindingType bindingType)
{
	const auto getDescriptor = [](Resource* resource, BindingType type)
	{
		switch (type)
		{
		case BindingType::CBV: return resource->CBV;
		case BindingType::SRV: return resource->SRV;
		case BindingType::UAV: return resource->UAV;
		default: NOT_IMPLEMENTED;
		}

		return D3D12_CPU_DESCRIPTOR_HANDLE{};
	};

	for (Resource* binding : bindings)
	{
		if (!binding) continue;

		const D3D12_CPU_DESCRIPTOR_HANDLE descriptor = getDescriptor(binding, bindingType);
		const DescriptorHeapGPU::Allocation gpuAlloc = heap.Alloc(page);
		Device::Get()->GetHandle()->CopyDescriptorsSimple(1, gpuAlloc.CPUHandle, descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}

static uint32_t CalcRootSignatureHash(const GraphicsState& state)
{
	uint32_t sigHash = 0xffffffff;
	for (const D3D12_STATIC_SAMPLER_DESC& samplerDesc : state.Samplers) sigHash = Hash::Crc32(sigHash, samplerDesc);
	sigHash = Hash::Crc32(sigHash, "CBV");
	for (uint32_t i = 0; i < state.Table.CBVs.size(); i++) if(state.Table.CBVs[i]) sigHash = Hash::Crc32(sigHash, i);
	sigHash = Hash::Crc32(sigHash, "SRV");
	for (uint32_t i = 0; i < state.Table.SRVs.size(); i++) if (state.Table.SRVs[i]) sigHash = Hash::Crc32(sigHash, i);
	sigHash = Hash::Crc32(sigHash, "UAV");
	for (uint32_t i = 0; i < state.Table.UAVs.size(); i++) if (state.Table.UAVs[i]) sigHash = Hash::Crc32(sigHash, i);
	return sigHash;
}

static ID3D12RootSignature* GetOrCreateRootSignature(const GraphicsState& state)
{
	uint32_t rootSignatureHash = CalcRootSignatureHash(state);
	if (RootSignatureCache.contains(rootSignatureHash)) return RootSignatureCache[rootSignatureHash].Get();

	ID3D12RootSignature* rootSignature = nullptr;

	const BindTable& table = state.Table;
	std::vector<D3D12_ROOT_PARAMETER> rootParameters;
	std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges[3];
	descriptorRanges[0] = CreateDescriptorRanges(table.CBVs, D3D12_DESCRIPTOR_RANGE_TYPE_CBV);
	descriptorRanges[1] = CreateDescriptorRanges(table.SRVs, D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
	descriptorRanges[2] = CreateDescriptorRanges(table.UAVs, D3D12_DESCRIPTOR_RANGE_TYPE_UAV);

	for (uint32_t i = 0; i < 3; i++)
	{
		if (descriptorRanges[i].empty()) continue;

		D3D12_ROOT_PARAMETER rootParamater;
		rootParamater.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParamater.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParamater.DescriptorTable.NumDescriptorRanges = (uint32_t)descriptorRanges[i].size();
		rootParamater.DescriptorTable.pDescriptorRanges = descriptorRanges[i].data();
		rootParameters.push_back(rootParamater);
	}

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.NumParameters = (uint32_t)rootParameters.size();
	rootSigDesc.pParameters = rootSigDesc.NumParameters == 0 ? nullptr : rootParameters.data();
	rootSigDesc.NumStaticSamplers = state.Samplers.size();
	rootSigDesc.pStaticSamplers = state.Samplers.size() == 0 ? nullptr : state.Samplers.data();
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (FAILED(hr))
	{
		const char* errorString = NULL;
		if (errorBlob) {
			errorString = (const char*)errorBlob->GetBufferPointer();
		}
		MessageBoxA(0, errorString, "Root signature serialization Error", MB_ICONERROR | MB_OK);
		ASSERT(0, "[GraphicsContext::SubmitState] Failed to create root signature");
	}

	Device* device = Device::Get();

	API_CALL(device->GetHandle()->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

	RootSignatureCache[rootSignatureHash] = rootSignature;

	return rootSignature;
}

static ID3D12PipelineState* GetOrCreatePSO(const GraphicsState& state, ID3D12RootSignature* rootSignature, uint32_t& psoHash)
{
	ID3D12PipelineState* pipelineState = nullptr;

	const bool useCompute = state.Compute.pShaderBytecode != nullptr;

	if (useCompute)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};
		pipeline.pRootSignature = rootSignature;
		pipeline.CS = state.Compute;
		pipeline.CachedPSO = { nullptr, 0 };
		pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		pipeline.NodeMask = 0;

		psoHash = Hash::Crc32(pipeline);
		if (PSOCache.contains(psoHash))
		{
			pipelineState = PSOCache[psoHash].Get();
		}
		else
		{
			API_CALL(Device::Get()->GetHandle()->CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&pipelineState)));
			PSOCache[psoHash] = pipelineState;
		}
	}
	else
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = state.Pipeline;
		pipeline.pRootSignature = rootSignature;
		psoHash = Hash::Crc32(pipeline);
		if (PSOCache.contains(psoHash))
		{
			pipelineState = PSOCache[psoHash].Get();
		}
		else
		{
			API_CALL(Device::Get()->GetHandle()->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&pipelineState)));
			PSOCache[psoHash] = pipelineState;
		}

		API_CALL(Device::Get()->GetHandle()->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&pipelineState)));
	}

	return pipelineState;
}

void ReleaseContextCache()
{
	RootSignatureCache.clear();
	PSOCache.clear();
}

void ApplyGraphicsState(GraphicsContext& context, const GraphicsState& state)
{
	Device* device = Device::Get();
	ID3D12GraphicsCommandList* cmdList = context.CmdList.Get();

	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	
	GPUAllocStrategy::Page pages[3];

	const bool useCompute = state.Compute.pShaderBytecode != nullptr;

	// Root Signature
	ID3D12RootSignature* rootSignature = GetOrCreateRootSignature(state);
	
	// Pipeline state
	uint32_t psoHash = 0;
	ID3D12PipelineState* pipelineState = GetOrCreatePSO(state, rootSignature, psoHash);

	bool rebindPSO = !context.BoundState.Valid;
	rebindPSO = rebindPSO || context.BoundState.PSOHash != psoHash;
	rebindPSO = rebindPSO || context.BoundState.IsCompute != useCompute;

	context.BoundState.Valid = true;
	context.BoundState.PSOHash = psoHash;
	context.BoundState.IsCompute = useCompute;

	if (rebindPSO)
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { context.MemContext.SRVHeap.Heap.Get() };
		cmdList->SetDescriptorHeaps(1, descriptorHeaps);

		if (useCompute)
		{
			cmdList->SetComputeRootSignature(rootSignature);
		}
		else
		{
			cmdList->SetGraphicsRootSignature(rootSignature);
		}
		cmdList->SetPipelineState(pipelineState);
	}

	if (!useCompute)
	{
		uint32_t numRts = 0;
		const D3D12_CPU_DESCRIPTOR_HANDLE* rtDesc = nullptr;
		const D3D12_CPU_DESCRIPTOR_HANDLE* dsDesc = nullptr;
		if (state.RenderTarget)
		{
			numRts = 1;
			rtDesc = &state.RenderTarget->RTV;
			GFX::Cmd::AddResourceTransition(barriers, state.RenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		if (state.DepthStencil)
		{
			dsDesc = &state.DepthStencil->DSV;
			GFX::Cmd::AddResourceTransition(barriers, state.DepthStencil, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}

		cmdList->OMSetRenderTargets(numRts, rtDesc, false, dsDesc);
		cmdList->OMSetStencilRef(state.StencilRef);
		cmdList->RSSetViewports(1, &state.Viewport);
		cmdList->RSSetScissorRects(1, &state.Scissor);
		cmdList->IASetPrimitiveTopology(state.Primitives);

		if (state.VertexBuffers.size())
		{
			std::vector<D3D12_VERTEX_BUFFER_VIEW> views;
			views.reserve(state.VertexBuffers.size());
			for (Buffer* buffer : state.VertexBuffers)
			{
				GFX::Cmd::AddResourceTransition(barriers, buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
				views.push_back({ buffer->GPUAddress, (uint32_t)buffer->ByteSize, (uint32_t)buffer->Stride });
			}
			cmdList->IASetVertexBuffers(0, views.size(), views.data());
		}

		if (state.IndexBuffer)
		{
			DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
			switch (state.IndexBuffer->Stride)
			{
			case 8:  dxgiFormat = DXGI_FORMAT_R8_UINT; break;
			case 16: dxgiFormat = DXGI_FORMAT_R16_UINT; break;
			case 32: dxgiFormat = DXGI_FORMAT_R32_UINT; break;
			default: NOT_IMPLEMENTED;
			}

			GFX::Cmd::AddResourceTransition(barriers, state.IndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
			D3D12_INDEX_BUFFER_VIEW ibv = { state.IndexBuffer->GPUAddress, (uint32_t)state.IndexBuffer->ByteSize, dxgiFormat };
			cmdList->IASetIndexBuffer(&ibv);
		}
	}

	// Update descriptor data
	{
		ASSERT(context.MemContext.SRVHeap.AllocStrategy.CanAllocate(3), "DescriptorHeapGPU memory overflow!");

		for (uint32_t i = 0; i < 3; i++)
		{
			pages[i] = context.MemContext.SRVHeap.AllocStrategy.AllocatePage();
			DeferredTrash::Put(&context.MemContext.SRVHeap, pages[i]);
		}

		UpdateDescriptorData(state.Table.CBVs, context.MemContext.SRVHeap, pages[0], BindingType::CBV);
		UpdateDescriptorData(state.Table.SRVs, context.MemContext.SRVHeap, pages[1], BindingType::SRV);
		UpdateDescriptorData(state.Table.UAVs, context.MemContext.SRVHeap, pages[2], BindingType::UAV);

		for (Resource* bind : state.Table.CBVs) GFX::Cmd::AddResourceTransition(barriers, bind, D3D12_RESOURCE_STATE_GENERIC_READ);
		for (Resource* bind : state.Table.SRVs) GFX::Cmd::AddResourceTransition(barriers, bind, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		for (Resource* bind : state.Table.UAVs) GFX::Cmd::AddResourceTransition(barriers, bind, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	// Bind root table
	{
		uint32_t nextSlot = 0;
		for (uint32_t i = 0; i < 3; i++)
		{
			if (pages[i].AllocationIndex == 0) continue;

			const DescriptorHeapGPU& heap = context.MemContext.SRVHeap;
			D3D12_GPU_DESCRIPTOR_HANDLE descriptor = { heap.HeapStartGPU };
			descriptor.ptr += pages[i].PageIndex * heap.PageSize;

			if (useCompute) cmdList->SetComputeRootDescriptorTable(nextSlot++, descriptor);
			else cmdList->SetGraphicsRootDescriptorTable(nextSlot++, descriptor);
		}
	}

	// Execute pending barriers
	{
		if (!barriers.empty())
		{
			cmdList->ResourceBarrier(barriers.size(), barriers.data());
		}
	}
}
