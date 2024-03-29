#include "PostprocessingRenderer.h"

#include <Engine/Render/Commands.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Device.h>
#include <Engine/Render/Texture.h>
#include <Engine/System/ApplicationConfiguration.h>

#include "Globals.h"
#include "Renderers/Util/ConstantBuffer.h"
#include "Renderers/Util/TextureDebugger.h"
#include "Scene/SceneGraph.h"

static const Float2 HaltonSequence[16] = { 
						{0.500000f,0.333333f},
						{0.250000f,0.666667f},
						{0.750000f,0.111111f},
						{0.125000f,0.444444f},
						{0.625000f,0.777778f},
						{0.375000f,0.222222f},
						{0.875000f,0.555556f},
						{0.062500f,0.888889f},
						{0.562500f,0.037037f},
						{0.312500f,0.370370f},
						{0.812500f,0.703704f},
						{0.187500f,0.148148f},
						{0.687500f,0.481481f},
						{0.437500f,0.814815f},
						{0.937500f,0.259259f},
						{0.031250f,0.592593f} };

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
	PROFILE_SECTION(context, "Postprocessing");
	
	Texture* hdrRT = colorInput;

	if (RenderSettings.AntialiasingMode == AntiAliasingMode::MSAA)
	{
		GFX::Cmd::ResolveTexture(context, hdrRT, m_ResolvedColor.get());
		hdrRT = m_ResolvedColor.get();
	}

	GraphicsState state;
	state.Table.SMPs[0] = { D3D12_FILTER_ANISOTROPIC , D3D12_TEXTURE_ADDRESS_MODE_WRAP };
	state.Table.SMPs[1] = { D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_WRAP };
	state.Table.SMPs[2] = { D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_BORDER, {0.0f, 0.0f, 0.0f, 0.0f} };

	// Bloom
	if (RenderSettings.Bloom.Enabled)
	{
		PROFILE_SECTION(context, "Bloom");

		// Prefilter
		{
			PROFILE_SECTION(context, "Prefilter");

			ConstantBuffer cb{};
			cb.Add(GetBloomInput(hdrRT));
			cb.Add(RenderSettings.Exposure);

			state.Table.CBVs[0] = cb.GetBuffer(context);
			state.Table.SRVs[0] = hdrRT;
			state.RenderTargets[0] = m_BloomTexturesDownsample[0].get();
			state.Shader = m_BloomShader.get();
			state.ShaderConfig = { "PREFILTER" };

			GFX::Cmd::DrawFC(context, state);
		}

		// Downsample
		{
			PROFILE_SECTION(context, "Downsample");
			state.Shader = m_BloomShader.get();
			state.ShaderConfig = { "DOWNSAMPLE" };
			for (uint32_t i = 1; i < BLOOM_NUM_SAMPLES; i++)
			{
				ConstantBuffer cb{};
				cb.Add(GetBloomInput(m_BloomTexturesDownsample[i - 1].get()));
				cb.Add(RenderSettings.Exposure);

				state.Table.CBVs[0] = cb.GetBuffer(context);
				state.Table.SRVs[0] = m_BloomTexturesDownsample[i - 1].get();
				state.RenderTargets[0] = m_BloomTexturesDownsample[i].get();
				GFX::Cmd::DrawFC(context, state);
			}
		}

		// Upsample
		{
			PROFILE_SECTION(context, "Upsample");
			
			ConstantBuffer cb{};
			cb.Add(GetBloomInput(m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES - 1].get()));
			cb.Add(RenderSettings.Exposure);

			state.Table.CBVs[0] = cb.GetBuffer(context);
			state.Table.SRVs[0] = m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES - 1].get();
			state.Table.SRVs[1] = m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES - 2].get();
			state.Shader = m_BloomShader.get();
			state.ShaderConfig = { "UPSAMPLE" };
			state.RenderTargets[0] = m_BloomTexturesUpsample[BLOOM_NUM_SAMPLES - 2].get();

			GFX::Cmd::DrawFC(context, state);

			for (int32_t i = BLOOM_NUM_SAMPLES - 3; i >= 0; i--)
			{
				ConstantBuffer cb1{};
				cb1.Add(GetBloomInput(m_BloomTexturesDownsample[i].get()));
				cb1.Add(RenderSettings.Exposure);
				state.Table.CBVs[0] = cb1.GetBuffer(context);
				state.Table.SRVs[0] = m_BloomTexturesDownsample[i + 1].get();
				state.Table.SRVs[1] = m_BloomTexturesDownsample[i].get();
				state.RenderTargets[0] = m_BloomTexturesUpsample[i].get();
				GFX::Cmd::DrawFC(context, state);
			}
		}
	}

	// Tonemapping
	{
		PROFILE_SECTION(context, "Tonemapping");

		ConstantBuffer cb{};
		cb.Add(RenderSettings.Exposure);

		state.Table.CBVs[0] = cb.GetBuffer(context);
		state.Table.SRVs[0] = hdrRT;
		state.Table.SRVs[1] = m_BloomTexturesUpsample[0].get();
		state.RenderTargets[0] = GetOutputTexture();
		state.Shader = m_PostprocessShader.get();

		std::vector<std::string> config{};
		config.push_back("TONEMAPPING");
		if (RenderSettings.Bloom.Enabled) config.push_back("APPLY_BLOOM");
		state.ShaderConfig = config;

		GFX::Cmd::DrawFC(context, state);

		Step();
	}

	// TAA
	if (RenderSettings.AntialiasingMode == AntiAliasingMode::TAA)
	{
		PROFILE_SECTION(context, "TAA");

		// TAA History
		GFX::Cmd::CopyToTexture(context, m_TAAHistory[0].get(), m_TAAHistory[1].get());
		GFX::Cmd::CopyToTexture(context, GetInputTexture(), m_TAAHistory[0].get());

		state.Table.SRVs[0] = m_TAAHistory[0].get();
		state.Table.SRVs[1] = m_TAAHistory[1].get();
		state.Table.SRVs[2] = motionVectorInput;
		state.RenderTargets[0] = GetOutputTexture();
		state.Shader = m_PostprocessShader.get();
		state.ShaderConfig = { "TAA" };

		GFX::Cmd::DrawFC(context, state);

		Step();
	}

	return GetInputTexture();
}

void PostprocessingRenderer::ReloadTextureResources(GraphicsContext& context)
{
	const uint32_t size[2] = { AppConfig.WindowWidth, AppConfig.WindowHeight };
	m_ResolvedColor = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF::RTV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT));
	m_TAAHistory[0] = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF::None));
	m_TAAHistory[1] = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF::None));
	m_PostprocessRT[0] = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF::RTV));
	m_PostprocessRT[1] = ScopedRef<Texture>(GFX::CreateTexture(size[0], size[1], RCF::RTV));

	GFX::SetDebugName(m_ResolvedColor.get(), "PostprocessingRenderer::ResolvedColor");
	GFX::SetDebugName(m_TAAHistory[0].get(), "PostprocessingRenderer::TAAHistory0");
	GFX::SetDebugName(m_TAAHistory[1].get(), "PostprocessingRenderer::TAAHistory1");
	GFX::SetDebugName(m_PostprocessRT[1].get(), "PostprocessingRenderer::PostprocessRT0");
	GFX::SetDebugName(m_PostprocessRT[1].get(), "PostprocessingRenderer::PostprocessRT1");

	TexDebugger.AddTexture("PostprocessRT", m_PostprocessRT[0].get());

	// Bloom
	const float aspect = (float)size[0] / size[1];
	uint32_t bloomTexSize[2] = { (uint32_t)(512 * aspect), 512 };
	for (uint32_t i = 0; i < BLOOM_NUM_SAMPLES; i++)
	{
		m_BloomTexturesDownsample[i] = ScopedRef<Texture>(GFX::CreateTexture(bloomTexSize[0], bloomTexSize[1], RCF::RTV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT));
		m_BloomTexturesUpsample[i] = ScopedRef<Texture>(GFX::CreateTexture(bloomTexSize[0], bloomTexSize[1], RCF::RTV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT));

		GFX::SetDebugName(m_BloomTexturesDownsample[i].get(), "PostprocessingRenderer::BloomTexturesDownsample" + std::to_string(i));
		GFX::SetDebugName(m_BloomTexturesUpsample[i].get(), "PostprocessingRenderer::BloomTexturesUpsample" + std::to_string(i));

		bloomTexSize[0] /= 2;
		bloomTexSize[1] /= 2;
	}

	// Camera
	SceneManager::Get().GetSceneGraph().MainCamera.UseJitter = RenderSettings.AntialiasingMode == AntiAliasingMode::TAA;
	for (uint32_t i = 0; i < 16; i++)
	{
		SceneManager::Get().GetSceneGraph().MainCamera.Jitter[i] = 2.0f * ((HaltonSequence[i] - Float2{ 0.5f, 0.5f }) / Float2((float) AppConfig.WindowWidth, (float) AppConfig.WindowHeight));
	}
}

