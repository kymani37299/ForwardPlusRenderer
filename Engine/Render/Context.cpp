#include "Context.h"

#include "Render/Device.h"
#include "Render/Commands.h"
#include "Render/Resource.h"
#include "Render/Texture.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Utility/Hash.h"

ContextManager* ContextManager::s_Instance = nullptr;

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

	BlendState = blend;
	RasterizerState = raster;
	DepthStencilState = depthStencil;

	CustomViewport = { 0.0f, 0.0f, (float)AppConfig.WindowWidth, (float)AppConfig.WindowHeight, 0.0f, 1.0f };
	CustomScissor = { 0,0, (long)AppConfig.WindowWidth, (long)AppConfig.WindowHeight };
	
	CommandSignature.ByteStride = 0;
	CommandSignature.NodeMask = 0;
	CommandSignature.NumArgumentDescs = 0;
	CommandSignature.pArgumentDescs = nullptr;
}

static std::vector<D3D12_DESCRIPTOR_RANGE> CreateDescriptorRanges(const BindVector<Resource*>& bindings, D3D12_DESCRIPTOR_RANGE_TYPE rangeType)
{
	std::vector<D3D12_DESCRIPTOR_RANGE> ranges;

	uint32_t descriptorsToBind = 0;
	for (uint32_t i = 0; i < bindings.size(); i++)
	{
		if (bindings[i]) descriptorsToBind++;

		if (!bindings[i] || i == bindings.size() - 1)
		{
			if (descriptorsToBind != 0)
			{
				uint32_t baseRegister = i - descriptorsToBind + 1;
				if (!bindings[i]) baseRegister--;

				D3D12_DESCRIPTOR_RANGE range;
				range.RangeType = rangeType;
				range.BaseShaderRegister = baseRegister;
				range.NumDescriptors = descriptorsToBind;
				range.RegisterSpace = 0;
				range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				ranges.push_back(range);

				descriptorsToBind = 0;
			}
		}
	}
	return ranges;
}

static std::vector<D3D12_DESCRIPTOR_RANGE> CreateDescriptorRanges(const BindVector<BindlessTable>& bindlessTables, D3D12_DESCRIPTOR_RANGE_TYPE rangeType)
{
	ASSERT(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV, "Bindless tables only supported for SRVs!");

	std::vector<D3D12_DESCRIPTOR_RANGE> ranges;

	for (const BindlessTable& table : bindlessTables)
	{
		ASSERT(table.RegisterSpace != 0 && table.DescriptorCount != 0, "table.RegisterSpace != 0 && table.DescriptorCount != 0");

		D3D12_DESCRIPTOR_RANGE range;
		range.RangeType = rangeType;
		range.BaseShaderRegister = 0;
		range.NumDescriptors = table.DescriptorCount;
		range.RegisterSpace = table.RegisterSpace;
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
		SMP
	};
}

static D3D12_PRIMITIVE_TOPOLOGY_TYPE ToPrimitiveTopologyType(const RenderPrimitiveType primitiveType)
{
	switch (primitiveType)
	{
	case RenderPrimitiveType::PointList:			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	case RenderPrimitiveType::LineList:
	case RenderPrimitiveType::LineListAdj:
	case RenderPrimitiveType::LineStrip:
	case RenderPrimitiveType::LineStripAdj:			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case RenderPrimitiveType::TriangleList:
	case RenderPrimitiveType::TriangleListAdj:
	case RenderPrimitiveType::TriangleStrip:
	case RenderPrimitiveType::TriangleStripAdj:		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case RenderPrimitiveType::PatchList:			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	default:
		NOT_IMPLEMENTED;
	}
	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
}

static D3D12_PRIMITIVE_TOPOLOGY ToPrimitiveTopology(const RenderPrimitiveType primitiveType, const uint32_t numControlPoints = 4)
{
	switch (primitiveType)
	{
	case RenderPrimitiveType::PointList:			return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case RenderPrimitiveType::LineList:				return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case RenderPrimitiveType::LineListAdj:			return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
	case RenderPrimitiveType::LineStrip:			return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case RenderPrimitiveType::LineStripAdj:			return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case RenderPrimitiveType::TriangleList:			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case RenderPrimitiveType::TriangleListAdj:		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
	case RenderPrimitiveType::TriangleStrip:		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case RenderPrimitiveType::TriangleStripAdj:		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
	case RenderPrimitiveType::PatchList:			return IntToEnum<D3D12_PRIMITIVE_TOPOLOGY>(EnumToInt(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST) + numControlPoints - 1);
	default:
		NOT_IMPLEMENTED;
	}
	return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

static uint32_t CalcRootSignatureHash(const GraphicsState& state)
{
	uint32_t sigHash = Hash::Crc32(state.PushConstantCount);
	sigHash = Hash::Crc32(sigHash, "CBV");
	for (uint32_t i = 0; i < state.Table.CBVs.size(); i++) if(state.Table.CBVs[i]) sigHash = Hash::Crc32(sigHash, i);
	sigHash = Hash::Crc32(sigHash, "SRV");
	for (uint32_t i = 0; i < state.Table.SRVs.size(); i++) if (state.Table.SRVs[i]) sigHash = Hash::Crc32(sigHash, i);
	sigHash = Hash::Crc32(sigHash, "UAV");
	for (uint32_t i = 0; i < state.Table.UAVs.size(); i++) if (state.Table.UAVs[i]) sigHash = Hash::Crc32(sigHash, i);
	sigHash = Hash::Crc32(sigHash, "SMP");
	for (uint32_t i = 0; i < state.Table.SMPs.size(); i++) sigHash = Hash::Crc32(sigHash, i);
	sigHash = Hash::Crc32(sigHash, "BTS");
	for (const BindlessTable& table : state.BindlessTables)
	{
		sigHash = Hash::Crc32(sigHash, "1");
		sigHash = Hash::Crc32(sigHash, table.RegisterSpace);
		sigHash = Hash::Crc32(sigHash, "2");
		sigHash = Hash::Crc32(sigHash, table.DescriptorCount);
		sigHash = Hash::Crc32(sigHash, "3");
		sigHash = Hash::Crc32(sigHash, table.DescriptorTable);
	}
	return sigHash;
}

static ID3D12RootSignature* GetOrCreateRootSignature(GraphicsContext& context, const GraphicsState& state)
{
	uint32_t rootSignatureHash = CalcRootSignatureHash(state);
	if (context.RootSignatureCache.contains(rootSignatureHash)) 
		return context.RootSignatureCache[rootSignatureHash].Get();

	ID3D12RootSignature* rootSignature = nullptr;

	const BindTable& table = state.Table;
	std::vector<D3D12_ROOT_PARAMETER> rootParameters;
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> descriptorRanges;

	if (!table.CBVs.empty()) descriptorRanges.push_back(CreateDescriptorRanges(table.CBVs, D3D12_DESCRIPTOR_RANGE_TYPE_CBV));
	if (!table.SRVs.empty()) descriptorRanges.push_back(CreateDescriptorRanges(table.SRVs, D3D12_DESCRIPTOR_RANGE_TYPE_SRV));
	if (!table.UAVs.empty()) descriptorRanges.push_back(CreateDescriptorRanges(table.UAVs, D3D12_DESCRIPTOR_RANGE_TYPE_UAV));

	if (!state.Table.SMPs.empty())
	{
		D3D12_DESCRIPTOR_RANGE samplerRange;
		samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		samplerRange.BaseShaderRegister = 0;
		samplerRange.NumDescriptors = (UINT)state.Table.SMPs.size();
		samplerRange.RegisterSpace = 0;
		samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		descriptorRanges.push_back({ samplerRange });
	}
	if(!state.BindlessTables.empty()) descriptorRanges.push_back(CreateDescriptorRanges(state.BindlessTables, D3D12_DESCRIPTOR_RANGE_TYPE_SRV));

	if (state.PushConstantCount != 0)
	{
		D3D12_ROOT_PARAMETER rootParamater;
		rootParamater.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParamater.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParamater.Constants.Num32BitValues = (uint32_t) state.PushConstantCount;
		rootParamater.Constants.RegisterSpace = 0;
		rootParamater.Constants.ShaderRegister = state.PushConstantBinding;
		rootParameters.push_back(rootParamater);
	}

	for (const auto& descriptors : descriptorRanges)
	{
		D3D12_ROOT_PARAMETER rootParamater;
		rootParamater.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParamater.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParamater.DescriptorTable.NumDescriptorRanges = (uint32_t)descriptors.size();
		rootParamater.DescriptorTable.pDescriptorRanges = descriptors.data();
		rootParameters.push_back(rootParamater);
	}

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.NumParameters = (uint32_t)rootParameters.size();
	rootSigDesc.pParameters = rootSigDesc.NumParameters == 0 ? nullptr : rootParameters.data();
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;
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

	context.RootSignatureCache[rootSignatureHash] = rootSignature;

	return rootSignature;
}

static ID3D12PipelineState* GetOrCreatePSO(GraphicsContext& context, const GraphicsState& state, ID3D12RootSignature* rootSignature, uint32_t& psoHash)
{
	ID3D12PipelineState* pipelineState = nullptr;
	const CompiledShader& compShader = GFX::GetCompiledShader(state.Shader, state.ShaderConfig, state.ShaderStages);

	if (state.ShaderStages & CS)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};
		pipeline.pRootSignature = rootSignature;
		pipeline.CS = compShader.Compute;
		pipeline.CachedPSO = { nullptr, 0 };
		pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		pipeline.NodeMask = 0;

		psoHash = Hash::Crc32(pipeline);
		if (context.PSOCache.contains(psoHash))
		{
			pipelineState = context.PSOCache[psoHash].Get();
		}
		else
		{
			API_CALL(Device::Get()->GetHandle()->CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&pipelineState)));
			context.PSOCache[psoHash] = pipelineState;
		}
	}
	else
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
		pipeline.pRootSignature = rootSignature;
		pipeline.StreamOutput = { nullptr, 0, nullptr, 0, 0 };
		pipeline.BlendState = state.BlendState;
		pipeline.SampleMask = UINT_MAX;
		pipeline.RasterizerState = state.RasterizerState;
		pipeline.DepthStencilState = state.DepthStencilState;
		pipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		pipeline.PrimitiveTopologyType = ToPrimitiveTopologyType(state.PrimitiveType);
		pipeline.NodeMask = 0;
		pipeline.CachedPSO = { nullptr, 0 };
		pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		pipeline.VS = compShader.Vertex;
		pipeline.HS = compShader.Hull;
		pipeline.DS = compShader.Domain;
		pipeline.GS = compShader.Geometry;
		pipeline.PS = compShader.Pixel;
		pipeline.InputLayout = (state.VertexBuffers.size() > 1) ? D3D12_INPUT_LAYOUT_DESC{ compShader.InputLayoutMultiInput.data(), (uint32_t)compShader.InputLayoutMultiInput.size() } : D3D12_INPUT_LAYOUT_DESC{ compShader.InputLayout.data(), (uint32_t)compShader.InputLayout.size() };
		pipeline.DSVFormat = state.DepthStencil ? state.DepthStencil->Format : DXGI_FORMAT_R24G8_TYPELESS;
		pipeline.NumRenderTargets = (UINT) state.RenderTargets.size();
		for (uint32_t i = 0; i < 8; i++) pipeline.RTVFormats[i] = i < pipeline.NumRenderTargets ? state.RenderTargets[i]->Format : DXGI_FORMAT_UNKNOWN;
		pipeline.SampleDesc.Count = state.RenderTargets.empty() ? (state.DepthStencil ? GetSampleCount(state.DepthStencil->CreationFlags) : 1) : GetSampleCount(state.RenderTargets[0]->CreationFlags);
		pipeline.SampleDesc.Quality = 0;

		psoHash = Hash::Crc32(pipeline);
		if (context.PSOCache.contains(psoHash))
		{
			pipelineState = context.PSOCache[psoHash].Get();
		}
		else
		{
			API_CALL(Device::Get()->GetHandle()->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&pipelineState)));
			context.PSOCache[psoHash] = pipelineState;
		}
	}
	return pipelineState;
}

static D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerDescriptor(GraphicsContext& context, const Sampler& sampler)
{
	D3D12_SAMPLER_DESC samplerDesc{};
	samplerDesc.AddressU = sampler.AddressMode;
	samplerDesc.AddressV = sampler.AddressMode;
	samplerDesc.AddressW = sampler.AddressMode;
	samplerDesc.Filter = sampler.Filter;
	samplerDesc.BorderColor[0] = sampler.BorderColor.x;
	samplerDesc.BorderColor[1] = sampler.BorderColor.y;
	samplerDesc.BorderColor[2] = sampler.BorderColor.z;
	samplerDesc.BorderColor[3] = sampler.BorderColor.w;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0;
	samplerDesc.MipLODBias = 0;
	
	const uint32_t samplerHash = Hash::Crc32(samplerDesc);
	if (!context.SamplerCache.contains(samplerHash))
	{
		context.SamplerCache[samplerHash] = Device::Get()->GetMemory().SMPHeap->Allocate();
		Device::Get()->GetHandle()->CreateSampler(&samplerDesc, context.SamplerCache[samplerHash].GetCPUHandle());
	}

	return context.SamplerCache[samplerHash].GetCPUHandle();
}

static DescriptorAllocation CreateDescriptorTable(GraphicsContext& context, const BindVector<Resource*>& bindings, BindingType bindingType)
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

		return DescriptorAllocation{};
	};

	std::vector<Resource*> bindingsToUpload{};
	for (Resource* binding : bindings)
	{
		if (binding) bindingsToUpload.push_back(binding);
	}
	DescriptorAllocation alloc = Device::Get()->GetMemory().SRVHeapGPU->AllocateTransient(bindingsToUpload.size());

	for (size_t i = 0; i < bindingsToUpload.size(); i++)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor = getDescriptor(bindingsToUpload[i], bindingType).GetCPUHandle();
		const D3D12_CPU_DESCRIPTOR_HANDLE dstDescriptor = alloc.GetCPUHandle(i);
		Device::Get()->GetHandle()->CopyDescriptorsSimple(1, dstDescriptor, srcDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	return alloc;
}

ID3D12CommandSignature* GraphicsContext::ApplyState(const GraphicsState& state)
{
	PROFILE_SECTION(*this, "ApplyState");

	Device* device = Device::Get();
	DeviceMemory& deviceMemory = Device::Get()->GetMemory();
	ID3D12GraphicsCommandList* cmdList = CmdList.Get();

	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	
	const bool useCompute = state.ShaderStages & CS;

	// Root Signature
	ID3D12RootSignature* rootSignature = GetOrCreateRootSignature(*this, state);
	
	// Pipeline state
	uint32_t psoHash = 0;
	ID3D12PipelineState* pipelineState = GetOrCreatePSO(*this, state, rootSignature, psoHash);

	const bool pipelineDirty = !BoundState.Valid || BoundState.PSOHash != psoHash;
	BoundState.Valid = true;
	BoundState.PSOHash = psoHash;

	if (pipelineDirty)
	{
		std::vector<ID3D12DescriptorHeap*> descriptorHeaps{};
		descriptorHeaps.push_back(deviceMemory.SRVHeapGPU->GetHeap());
		descriptorHeaps.push_back(deviceMemory.SMPHeapGPU->GetHeap());
		cmdList->SetDescriptorHeaps((UINT) descriptorHeaps.size(), descriptorHeaps.data());

		if (useCompute) cmdList->SetComputeRootSignature(rootSignature);
		else cmdList->SetGraphicsRootSignature(rootSignature);
		cmdList->SetPipelineState(pipelineState);
	}

	if (!useCompute)
	{
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtDescs{};
		rtDescs.reserve(state.RenderTargets.size());
		const D3D12_CPU_DESCRIPTOR_HANDLE* dsDescPtr = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE dsDesc;
		
		for (Texture* rt : state.RenderTargets)
		{
			ASSERT(TestFlag(rt->CreationFlags, RCF::RTV), "Texture must have RCF_Bind_RTV in order to be used as a render target!");
			GFX::Cmd::AddResourceTransition(barriers, rt, D3D12_RESOURCE_STATE_RENDER_TARGET);
			rtDescs.push_back(rt->RTV.GetCPUHandle());
		}

		if (state.DepthStencil)
		{
			ASSERT(TestFlag(state.DepthStencil->CreationFlags, RCF::DSV), "Texture must have RCF_Bind_DSV in order to be used as a depth stencil!");
			GFX::Cmd::AddResourceTransition(barriers, state.DepthStencil, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			dsDesc = state.DepthStencil->DSV.GetCPUHandle();
			dsDescPtr = &dsDesc;
		}

		cmdList->OMSetRenderTargets((UINT) rtDescs.size(), rtDescs.empty() ? nullptr : rtDescs.data(), false, dsDescPtr);
		cmdList->OMSetStencilRef(state.StencilRef);
		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
		D3D12_RECT scissor = { 0, 0, 0, 0 };
		if (state.DepthStencil)
		{
			viewport.Width = (float)state.DepthStencil->Width;
			viewport.Height = (float)state.DepthStencil->Height;
			scissor.right = (long)state.DepthStencil->Width;
			scissor.bottom = (long)state.DepthStencil->Height;
		}
		if (!state.RenderTargets.empty())
		{
			viewport.Width = (float)state.RenderTargets[0]->Width;
			viewport.Height = (float)state.RenderTargets[0]->Height;
			scissor.right = (long)state.RenderTargets[0]->Width;
			scissor.bottom = (long)state.RenderTargets[0]->Height;
		}
		if (state.UseCustomViewport)
			viewport = state.CustomViewport;

		if (state.UseCustomScissor)
			scissor = state.CustomScissor;

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissor);
		cmdList->IASetPrimitiveTopology(ToPrimitiveTopology(state.PrimitiveType, state.NumControlPoints));

		if (state.VertexBuffers.size())
		{
			std::vector<D3D12_VERTEX_BUFFER_VIEW> views;
			views.reserve(state.VertexBuffers.size());
			for (Buffer* buffer : state.VertexBuffers)
			{
				GFX::Cmd::AddResourceTransition(barriers, buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
				views.push_back({ buffer->GPUAddress, (uint32_t)buffer->ByteSize, (uint32_t)buffer->Stride });
			}
			cmdList->IASetVertexBuffers(0, (UINT) views.size(), views.data());
		}

		if (state.IndexBuffer)
		{
			DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
			switch (state.IndexBuffer->Stride)
			{
			case 1:  dxgiFormat = DXGI_FORMAT_R8_UINT; break;
			case 2: dxgiFormat = DXGI_FORMAT_R16_UINT; break;
			case 4: dxgiFormat = DXGI_FORMAT_R32_UINT; break;
			default: NOT_IMPLEMENTED;
			}

			GFX::Cmd::AddResourceTransition(barriers, state.IndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
			D3D12_INDEX_BUFFER_VIEW ibv = { state.IndexBuffer->GPUAddress, (uint32_t)state.IndexBuffer->ByteSize, dxgiFormat };
			cmdList->IASetIndexBuffer(&ibv);
		}
	}

	// Setup descriptor tables
	{
		std::vector<DescriptorAllocation> descriptorTables;
		
		if (!state.Table.CBVs.empty()) descriptorTables.push_back(CreateDescriptorTable(*this, state.Table.CBVs, BindingType::CBV));
		if (!state.Table.SRVs.empty()) descriptorTables.push_back(CreateDescriptorTable(*this, state.Table.SRVs, BindingType::SRV));
		if (!state.Table.UAVs.empty()) descriptorTables.push_back(CreateDescriptorTable(*this, state.Table.UAVs, BindingType::UAV));

		if (!state.Table.SMPs.empty())
		{
			const uint32_t samplerTableIndex = (uint32_t) descriptorTables.size();
			descriptorTables.push_back(deviceMemory.SMPHeapGPU->AllocateTransient(state.Table.SMPs.size()));

			for (size_t i = 0; i < state.Table.SMPs.size(); i++)
			{
				const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor = GetSamplerDescriptor(*this, state.Table.SMPs[i]);
				const D3D12_CPU_DESCRIPTOR_HANDLE dstDescriptor = descriptorTables[samplerTableIndex].GetCPUHandle(i);
				Device::Get()->GetHandle()->CopyDescriptorsSimple(1, dstDescriptor, srcDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
		}

		for (const BindlessTable& table : state.BindlessTables)
		{
			descriptorTables.push_back(table.DescriptorTable);
		}

		// Add resource transitions
		for (Resource* bind : state.Table.CBVs) GFX::Cmd::AddResourceTransition(barriers, bind, D3D12_RESOURCE_STATE_GENERIC_READ);
		for (Resource* bind : state.Table.SRVs) GFX::Cmd::AddResourceTransition(barriers, bind, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		for (Resource* bind : state.Table.UAVs) GFX::Cmd::AddResourceTransition(barriers, bind, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// Bind to root table
		uint32_t nextSlot = state.PushConstantCount > 0 ? 1 : 0;
		for (const DescriptorAllocation& descriptorTable : descriptorTables)
		{
			const D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorTable.GetGPUHandle();
			if (useCompute) cmdList->SetComputeRootDescriptorTable(nextSlot++, descriptorHandle);
			else cmdList->SetGraphicsRootDescriptorTable(nextSlot++, descriptorHandle);
		}
	}

	// Execute pending barriers
	{
		if (!barriers.empty())
		{
			cmdList->ResourceBarrier((UINT) barriers.size(), barriers.data());
		}
	}

	// Create command signature
	ID3D12CommandSignature* commandSignature = nullptr;
	if (state.CommandSignature.ByteStride != 0)
	{
		// TODO: Cache signatures
		ID3D12RootSignature* rootSig = nullptr;
		if (state.CommandSignature.NumArgumentDescs > 1) rootSig = rootSignature;
		API_CALL(Device::Get()->GetHandle()->CreateCommandSignature(&state.CommandSignature, rootSig, IID_PPV_ARGS(&commandSignature)));
		GFX::Cmd::Delete(*this, commandSignature);
	}
	return commandSignature;
}

GraphicsContext::~GraphicsContext()
{
	GFX::Cmd::BeginRecording(*this);

	for (const auto& [key, value] : SamplerCache)
		GFX::Cmd::Delete(*this, value);

	StagingResources.ClearTransientTextures(*this);

	GFX::Cmd::EndRecordingAndSubmit(*this);
	GFX::Cmd::WaitToFinish(*this);

	// Just to clear resources from the destructor
	GFX::Cmd::BeginRecording(*this);
	GFX::Cmd::EndRecordingAndSubmit(*this);
	GFX::Cmd::WaitToFinish(*this);
}

static uint32_t GetHash(const StagingResourcesContext::StagingTextureRequest& request)
{
	uint32_t h = Hash::Crc32(request.Width);
	h = Hash::Crc32(h, request.Height);
	h = Hash::Crc32(h, request.NumMips);
	h = Hash::Crc32(h, request.CreationFlags);
	h = Hash::Crc32(h, request.Format);
	return h;
}

StagingResourcesContext::StagingTexture* StagingResourcesContext::GetTransientTexture(const StagingTextureRequest& request)
{
	const uint32_t hash = GetHash(request);

	if (!m_TransientStagingTextures.contains(hash))
	{
		StagingTexture* tex = new StagingTexture{};
		tex->TextureResource = GFX::CreateTexture(request.Width, request.Height, request.CreationFlags, request.NumMips, request.Format);
		GFX::SetDebugName(tex->TextureResource, "StagingResourcesContext::Texture");
		m_TransientStagingTextures[hash] = tex;
	}
	return m_TransientStagingTextures[hash];
}

void StagingResourcesContext::ClearTransientTextures(GraphicsContext& context)
{
	for (const auto it : m_TransientStagingTextures)
	{
		GFX::Cmd::Delete(context, it.second->TextureResource);
		delete it.second;
	}
	m_TransientStagingTextures.clear();
}

static GraphicsContext* CreateGraphicsContext()
{
	GraphicsContext* context = new GraphicsContext{};

	ID3D12Device* device = Device::Get()->GetHandle();
	API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(context->CmdAlloc.GetAddressOf())));
	API_CALL(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, context->CmdAlloc.Get(), nullptr /* Initial PSO */, IID_PPV_ARGS(context->CmdList.GetAddressOf())));
	API_CALL(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(context->CmdFence.Handle.GetAddressOf())));
	context->CmdFence.Value = 0;

	GFX::Cmd::EndRecordingAndSubmit(*context);

	return context;
}

ContextManager::ContextManager()
{
	for (uint32_t i = 0; i < Device::IN_FLIGHT_FRAME_COUNT; i++)
	{
		m_FrameContexts[i] = ScopedRef<GraphicsContext>(CreateGraphicsContext());
	}
	m_CreationContext = ScopedRef<GraphicsContext>(CreateGraphicsContext());
}

void ContextManager::Flush()
{
	m_CreationMutex.Lock();
	for (uint32_t i = 0; i < Device::IN_FLIGHT_FRAME_COUNT; i++)
	{
		GFX::Cmd::WaitToFinish(*m_FrameContexts[i]);
	}
	for (ScopedRef<GraphicsContext>& workerContext : m_WorkerContexts)
	{
		GFX::Cmd::WaitToFinish(*workerContext);
	}
	GFX::Cmd::WaitToFinish(*m_CreationContext);
	m_CreationMutex.Unlock();
}

GraphicsContext& ContextManager::CreateWorkerContext()
{
	GraphicsContext* context = CreateGraphicsContext();
	m_CreationMutex.Lock();
	m_WorkerContexts.push_back(ScopedRef<GraphicsContext>{context});
	m_CreationMutex.Unlock();
	return *context;
}