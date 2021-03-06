#include "PostprocessingRenderer.h"

#include <Engine/Render/Commands.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Device.h>
#include <Engine/Render/Texture.h>
#include <Engine/System/ApplicationConfiguration.h>

#include "Globals.h"
#include "Renderers/Util/ConstantManager.h"
#include "Renderers/Util/SamplerManager.h"
#include "Scene/SceneGraph.h"

static const Float2 HaltonSequence[16] = { {0.500000,0.333333},
						{0.250000,0.666667},
						{0.750000,0.111111},
						{0.125000,0.444444},
						{0.625000,0.777778},
						{0.375000,0.222222},
						{0.875000,0.555556},
						{0.062500,0.888889},
						{0.562500,0.037037},
						{0.312500,0.370370},
						{0.812500,0.703704},
						{0.187500,0.148148},
						{0.687500,0.481481},
						{0.437500,0.814815},
						{0.937500,0.259259},
						{0.031250,0.592593} };

struct BloomInputRenderData
{
	DirectX::XMFLOAT4 SampleScale;
	DirectX::XMFLOAT2A TexelSize;
	float Treshold;
	float Knee;
};

static BloomInputRenderData GetBloomInput(Texture* texture)
{
	BloomInputRenderData input{};
	input.SampleScale = RenderSettings.Bloom.SamplingScale.ToXMF();
	input.Treshold = RenderSettings.Bloom.FTheshold;
	input.Knee = RenderSettings.Bloom.FKnee;
	input.TexelSize.x = 1.0f / texture->Width;
	input.TexelSize.y = 1.0f / texture->Height;
	return input;
}

PostprocessingRenderer::PostprocessingRenderer()
{

}

PostprocessingRenderer::~PostprocessingRenderer()
{

}

void PostprocessingRenderer::Init(GraphicsContext& context)
{
	m_PostprocessShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/postprocessing.hlsl"));
	m_BloomShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/bloom.hlsl"));
}

Texture* PostprocessingRenderer::Process(GraphicsContext& context, Texture* colorInput, Texture* motionVectorInput)
{
	GFX::Cmd::MarkerBegin(context, "Postprocessing");

	Texture* hdrRT = colorInput;

	if (RenderSettings.AntialiasingMode == AntiAliasingMode::MSAA)
	{
		GFX::Cmd::ResolveTexture(context, hdrRT, m_ResolvedColor.get());
		hdrRT = m_ResolvedColor.get();
	}

	GraphicsState state;
	state.Table.SRVs.resize(3);
	state.Table.CBVs.resize(1);
	SSManager.Bind(state);

	// Bloom
	if (RenderSettings.Bloom.Enabled)
	{
		GFX::Cmd::MarkerBegin(context, "Bloom");

		// Prefilter
		{
			GFX::Cmd::MarkerBegin(context, "Prefilter");

			CBManager.Clear();
			CBManager.Add(GetBloomInput(hdrRT));
			CBManager.Add(RenderSettings.Exposure);
			state.Table.CBVs[0] = CBManager.GetBuffer();
			state.Table.SRVs[0] = hdrRT;

			GFX::Cmd::BindRenderTarget(state, m_BloomTexturesDownsample[0].get());
			GFX::Cmd::BindShader(state, m_BloomShader.get(), VS | PS, {"PREFILTER"});
			GFX::Cmd::DrawFC(context, state);
			GFX::Cmd::MarkerEnd(context);
		}

		// Downsample
		{
			GFX::Cmd::MarkerBegin(context, "Downsample");
			GFX::Cmd::BindShader(state, m_BloomShader.get(), VS | PS, {"DOWNSAMPLE"});
			for (uint32_t i = 1; i < BLOOM_NUM_SAMPLES; i++)
			{
				CBManager.Clear();
				CBManager.Add(GetBloomInput(m_BloomTexturesDownsample[i - 1].get()));
				CBManager.Add(RenderSettings.Exposure);
				state.Table.CBVs[0] = CBManager.GetBuffer();
				state.Table.SRVs[0] = m_BloomTexturesDownsample[i - 1].get();

				GFX::Cmd::BindRenderTarget(state, m_BloomTexturesDownsample[i].get());
				GFX::Cmd::DrawFC(context, state);
			}
			GFX::Cmd::MarkerEnd(context);
		}

		// Upsample
		{
			GFX::Cmd::MarkerBegin(context, "Upsample");
			GFX::Cmd::BindShader(state, m_BloomShader.get(), VS | PS, {"UPSAMPLE"});

			CBManager.Clear();
			CBManager.Add(GetBloomInput(m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES - 1].get()));
			CBManager.Add(RenderSettings.Exposure);
			state.Table.CBVs[0] = CBManager.GetBuffer();
			state.Table.SRVs[0] = m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES - 1].get();
			state.Table.SRVs[1] = m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES - 2].get();

			GFX::Cmd::BindRenderTarget(state, m_BloomTexturesUpsample[BLOOM_NUM_SAMPLES - 2].get());
			GFX::Cmd::DrawFC(context, state);

			for (int32_t i = BLOOM_NUM_SAMPLES - 3; i >= 0; i--)
			{
				CBManager.Clear();
				CBManager.Add(GetBloomInput(m_BloomTexturesDownsample[i].get()));
				CBManager.Add(RenderSettings.Exposure);
				state.Table.CBVs[0] = CBManager.GetBuffer();
				state.Table.SRVs[0] = m_BloomTexturesDownsample[i + 1].get();
				state.Table.SRVs[1] = m_BloomTexturesDownsample[i].get();

				GFX::Cmd::BindRenderTarget(state, m_BloomTexturesUpsample[i].get());
				GFX::Cmd::DrawFC(context, state);
			}

			GFX::Cmd::MarkerEnd(context);
		}

		GFX::Cmd::MarkerEnd(context);
	}

	// Tonemapping
	{
		GFX::Cmd::MarkerBegin(context, "Tonemapping");

		CBManager.Clear();
		CBManager.Add(RenderSettings.Exposure);
		state.Table.CBVs[0] = CBManager.GetBuffer();
		state.Table.SRVs[0] = hdrRT;
		state.Table.SRVs[1] = m_BloomTexturesUpsample[0].get();

		std::vector<std::string> config{};
		config.push_back("TONEMAPPING");
		if (RenderSettings.Bloom.Enabled) config.push_back("APPLY_BLOOM");

		GFX::Cmd::BindRenderTarget(state, GetOutputTexture());
		GFX::Cmd::BindShader(state, m_PostprocessShader.get(), PS | VS, config);
		GFX::Cmd::DrawFC(context, state);
		GFX::Cmd::MarkerEnd(context);

		Step();
	}

	// TAA
	if (RenderSettings.AntialiasingMode == AntiAliasingMode::TAA)
	{
		// TAA History
		GFX::Cmd::CopyToTexture(context, m_TAAHistory[0].get(), m_TAAHistory[1].get());
		GFX::Cmd::CopyToTexture(context, GetInputTexture(), m_TAAHistory[0].get());

		state.Table.SRVs[0] = m_TAAHistory[0].get();
		state.Table.SRVs[1] = m_TAAHistory[1].get();
		state.Table.SRVs[2] = motionVectorInput;

		GFX::Cmd::MarkerBegin(context, "TAA");
		GFX::Cmd::BindRenderTarget(state, GetOutputTexture());
		GFX::Cmd::BindShader(state, m_PostprocessShader.get(), VS | PS, {"TAA"});
		GFX::Cmd::DrawFC(context, state);
		GFX::Cmd::MarkerEnd(context);

		Step();
	}

	GFX::Cmd::MarkerEnd(context);

	return GetInputTexture();
}

void PostprocessingRenderer::ReloadTextureResources(GraphicsContext& context)
{
	const uint32_t size[2] = { AppConfig.WindowWidth, AppConfig.WindowHeight };
	m_ResolvedColor = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT));
	m_TAAHistory[0] = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF_None));
	m_TAAHistory[1] = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF_None));
	m_PostprocessRT[0] = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV));
	m_PostprocessRT[1] = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV));

	GFX::SetDebugName(m_ResolvedColor.get(), "PostprocessingRenderer::ResolvedColor");
	GFX::SetDebugName(m_TAAHistory[0].get(), "PostprocessingRenderer::TAAHistory0");
	GFX::SetDebugName(m_TAAHistory[1].get(), "PostprocessingRenderer::TAAHistory1");
	GFX::SetDebugName(m_PostprocessRT[1].get(), "PostprocessingRenderer::PostprocessRT0");
	GFX::SetDebugName(m_PostprocessRT[1].get(), "PostprocessingRenderer::PostprocessRT1");

	// Bloom
	const float aspect = (float)size[0] / size[1];
	uint32_t bloomTexSize[2] = { 512 * aspect, 512 };
	for (uint32_t i = 0; i < BLOOM_NUM_SAMPLES; i++)
	{
		m_BloomTexturesDownsample[i] = ScopedRef<Texture>(GFX::CreateTexture(bloomTexSize[0], bloomTexSize[1], RCF_Bind_RTV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT));
		m_BloomTexturesUpsample[i] = ScopedRef<Texture>(GFX::CreateTexture(bloomTexSize[0], bloomTexSize[1], RCF_Bind_RTV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT));

		GFX::SetDebugName(m_BloomTexturesDownsample[i].get(), "PostprocessingRenderer::BloomTexturesDownsample" + std::to_string(i));
		GFX::SetDebugName(m_BloomTexturesUpsample[i].get(), "PostprocessingRenderer::BloomTexturesUpsample" + std::to_string(i));

		bloomTexSize[0] /= 2;
		bloomTexSize[1] /= 2;
	}

	// Camera
	MainSceneGraph->MainCamera.UseJitter = RenderSettings.AntialiasingMode == AntiAliasingMode::TAA;
	for (uint32_t i = 0; i < 16; i++)
	{
		MainSceneGraph->MainCamera.Jitter[i] = 2.0f * ((HaltonSequence[i] - Float2{ 0.5f, 0.5f }) / Float2(AppConfig.WindowWidth, AppConfig.WindowHeight));
	}
}

