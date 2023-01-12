#include "VertexPipeline.h"

#include <Engine/Render/Buffer.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Commands.h>
#include <Engine/Utility/MathUtility.h>

#include "Globals.h"
#include "Renderers/Util/ConstantManager.h"
#include "Scene/SceneGraph.h"
#include "Shaders/shared_definitions.h"

VertexPipeline* VertPipeline = nullptr;

VertexPipeline::VertexPipeline()
{

}

VertexPipeline::~VertexPipeline()
{

}

static constexpr uint32_t INDIRECT_ARGUMENTS_STRIDE = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) + sizeof(uint32_t);

void VertexPipeline::Init(GraphicsContext& context)
{
	m_IndirectArgumentsBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(INDIRECT_ARGUMENTS_STRIDE, INDIRECT_ARGUMENTS_STRIDE, RCF_Bind_UAV));
	m_IndirectArgumentsCountBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_UAV));
	m_PrepareArgsShader = ScopedRef<Shader>(new Shader{ "Forward+/Shaders/geometry_culling.hlsl" });
}

void VertexPipeline::Draw(GraphicsContext& context, GraphicsState& state, RenderGroup& rg, RenderGroupCullingData& cullingData)
{
	if (rg.Drawables.GetSize() == 0u) return;

	if (RenderSettings.Culling.GeometryCullingOnCPU)
	{
		Draw_CPU(context, state, rg, cullingData);
	}
	else
	{
		Draw_GPU(context, state, rg, cullingData);
	}
}

void VertexPipeline::Draw_CPU(GraphicsContext& context, GraphicsState& state, RenderGroup& rg, RenderGroupCullingData& cullingData)
{
	GFX::Cmd::MarkerBegin(context, "VertexPipeline::Execute");

	rg.SetupPipelineInputs(state);
	state.VertexBuffers[0] = rg.MeshData.GetVertexBuffer();
	state.IndexBuffer = rg.MeshData.GetIndexBuffer();
	state.PushConstantCount = 1;
	context.ApplyState(state);
	
	BindVector<uint32_t> pushConstantValues;
	for (uint32_t i = 0; i < rg.Drawables.GetSize(); i++)
	{
		if (!cullingData.VisibilityMask.Get(i)) continue;

		const Drawable& d = rg.Drawables[i];
		const Mesh& m = rg.Meshes[d.MeshIndex];

		pushConstantValues[0] = i;
		GFX::Cmd::SetPushConstants(state.ShaderStages, context, pushConstantValues);
		context.CmdList->DrawIndexedInstanced(m.IndexCount, 1, m.IndexOffset, m.VertOffset, 0);
	}

	GFX::Cmd::MarkerEnd(context);
}

void VertexPipeline::Draw_GPU(GraphicsContext& context, GraphicsState& state, RenderGroup& rg, RenderGroupCullingData& cullingData)
{
	// Prepare args
	{
		GFX::ExpandBuffer(context, m_IndirectArgumentsBuffer.get(), INDIRECT_ARGUMENTS_STRIDE * (UINT) rg.Drawables.GetSize());

		const uint32_t clearValue = 0;
		GFX::Cmd::UploadToBuffer(context, m_IndirectArgumentsCountBuffer.get(), 0, &clearValue, 0, sizeof(uint32_t));

		GraphicsState prepareState{};
		prepareState.Table.SRVs[0] = rg.Drawables.GetBuffer();
		prepareState.Table.SRVs[1] = rg.Meshes.GetBuffer();
		prepareState.Table.SRVs[2] = cullingData.VisibilityMaskBuffer.get();
		prepareState.Table.UAVs[0] = m_IndirectArgumentsBuffer.get();
		prepareState.Table.UAVs[1] = m_IndirectArgumentsCountBuffer.get();

		CBManager.Clear();
		CBManager.Add(rg.Drawables.GetSize());
		prepareState.Table.CBVs[0] = CBManager.GetBuffer();

		std::vector<std::string> config{};
		config.push_back("PREPARE_ARGUMENTS");
		
		prepareState.Shader = m_PrepareArgsShader.get();
		prepareState.ShaderStages = CS;
		prepareState.ShaderConfig = config;
		context.ApplyState(prepareState);
		context.CmdList->Dispatch(MathUtility::CeilDiv(rg.Drawables.GetSize(), (uint32_t) WAVESIZE), 1, 1);
	}

	// Execute draws
	{
		D3D12_INDIRECT_ARGUMENT_DESC indirectArguments[2];
		indirectArguments[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
		indirectArguments[0].Constant.DestOffsetIn32BitValues = 0;
		indirectArguments[0].Constant.RootParameterIndex = 0;
		indirectArguments[0].Constant.Num32BitValuesToSet = 1;
		indirectArguments[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		state.CommandSignature.ByteStride = INDIRECT_ARGUMENTS_STRIDE;
		state.CommandSignature.NumArgumentDescs = 2;
		state.CommandSignature.pArgumentDescs = indirectArguments;
		state.CommandSignature.NodeMask = 0;

		rg.SetupPipelineInputs(state);
		state.VertexBuffers[0] = rg.MeshData.GetVertexBuffer();
		state.IndexBuffer = rg.MeshData.GetIndexBuffer();
		state.PushConstantCount = 1;

		GFX::Cmd::TransitionResource(context, m_IndirectArgumentsCountBuffer.get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		GFX::Cmd::TransitionResource(context, m_IndirectArgumentsBuffer.get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

		ID3D12CommandSignature* commandSignature = context.ApplyState(state);
		context.CmdList->ExecuteIndirect(commandSignature, (UINT) rg.Drawables.GetSize(), m_IndirectArgumentsBuffer->Handle.Get(), 0, m_IndirectArgumentsCountBuffer->Handle.Get(), 0u);
	}
}
