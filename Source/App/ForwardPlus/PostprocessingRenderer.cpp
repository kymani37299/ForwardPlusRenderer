#include "PostprocessingRenderer.h"

#include "Core/SceneGraph.h"
#include "Render/Commands.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/Device.h"
#include "Render/Texture.h"
#include "Gui/GUI_Implementations.h"
#include "System/ApplicationConfiguration.h"



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


static void SetupAndBindBloomInput(ID3D11DeviceContext* context, TextureID targetTex)
{
	struct BloomInputCB
	{
		DirectX::XMFLOAT4 SampleScale;
		DirectX::XMFLOAT2A TexelSize;
		float Treshold;
		float Knee;
	};

	static BufferID BloomInputBuffer;
	if (!BloomInputBuffer.Valid()) BloomInputBuffer = GFX::CreateConstantBuffer<BloomInputCB>();

	const Texture& tex = GFX::Storage::GetTexture(targetTex);

	BloomInputCB inputCB{};
	inputCB.SampleScale = PostprocessSettings.BloomSampleScale.ToXMF();
	inputCB.Treshold = PostprocessSettings.BloomTreshold;
	inputCB.Knee = PostprocessSettings.BloomKnee;
	inputCB.TexelSize.x = 1.0f / tex.Width;
	inputCB.TexelSize.y = 1.0f / tex.Height;

	GFX::Cmd::UploadToBuffer(context, BloomInputBuffer, 0, &inputCB, 0, sizeof(BloomInputCB));
	GFX::Cmd::BindCBV<PS>(context, BloomInputBuffer, 0);
}

void PostprocessingRenderer::Init(ID3D11DeviceContext* context)
{
	m_PostprocessShader = GFX::CreateShader("Source/Shaders/postprocessing.hlsl");
	m_BloomShader = GFX::CreateShader("Source/Shaders/bloom.hlsl");
}

TextureID PostprocessingRenderer::Process(ID3D11DeviceContext* context, TextureID colorInput, TextureID depthInput, TextureID motionVectorInput)
{
	GFX::Cmd::MarkerBegin(context, "Postprocessing");

	// Update settings buffer
	{
		struct PostprocessingSettngsCB
		{
			float Exposure;
		};

		if (!m_PostprocessingSettingsBuffer.Valid())
			m_PostprocessingSettingsBuffer = GFX::CreateConstantBuffer<PostprocessingSettngsCB>();

		PostprocessingSettngsCB cb{};
		cb.Exposure = PostprocessSettings.Exposure;

		GFX::Cmd::UploadToBuffer(context, m_PostprocessingSettingsBuffer, 0, &cb, 0, sizeof(PostprocessingSettngsCB));
	}

	TextureID hdrRT = colorInput;

	if (PostprocessSettings.AntialiasingMode == AntiAliasingMode::MSAA)
	{
		GFX::Cmd::ResolveTexture(context, hdrRT, m_ResolvedColor);
		hdrRT = m_ResolvedColor;
	}

	// Bloom
	if(PostprocessSettings.EnableBloom)
	{
		GFX::Cmd::MarkerBegin(context, "Bloom");
		GFX::Cmd::SetupStaticSamplers<PS>(context);

		// Prefilter
		{
			GFX::Cmd::MarkerBegin(context, "Prefilter");
			GFX::Cmd::BindRenderTarget(context, m_BloomTexturesDownsample[0]);
			GFX::Cmd::BindShader<VS | PS>(context, m_BloomShader, { "PREFILTER" });
			GFX::Cmd::BindCBV<PS>(context, m_PostprocessingSettingsBuffer, 1);
			GFX::Cmd::BindSRV<PS>(context, hdrRT, 0);
			SetupAndBindBloomInput(context, hdrRT);
			GFX::Cmd::DrawFC(context);
			GFX::Cmd::MarkerEnd(context);
		}

		// Downsample
		{
			GFX::Cmd::MarkerBegin(context, "Downsample");
			GFX::Cmd::BindShader<VS | PS>(context, m_BloomShader, { "DOWNSAMPLE" });
			for (uint32_t i = 1; i < BLOOM_NUM_SAMPLES; i++)
			{
				GFX::Cmd::BindRenderTarget(context, m_BloomTexturesDownsample[i]);
				GFX::Cmd::BindSRV<PS>(context, m_BloomTexturesDownsample[i-1], 0);
				SetupAndBindBloomInput(context, m_BloomTexturesDownsample[i - 1]);
				GFX::Cmd::DrawFC(context);
			}
			GFX::Cmd::MarkerEnd(context);
		}

		// Upsample
		{
			GFX::Cmd::MarkerBegin(context, "Upsample");
			GFX::Cmd::BindShader<VS | PS>(context, m_BloomShader, { "UPSAMPLE" });

			GFX::Cmd::BindRenderTarget(context, m_BloomTexturesUpsample[BLOOM_NUM_SAMPLES - 2]);
			GFX::Cmd::BindSRV<PS>(context, m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES - 1], 0);
			GFX::Cmd::BindSRV<PS>(context, m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES - 2], 1);
			SetupAndBindBloomInput(context, m_BloomTexturesDownsample[BLOOM_NUM_SAMPLES - 1]);
			GFX::Cmd::DrawFC(context);

			for (int32_t i = BLOOM_NUM_SAMPLES - 3; i >= 0; i--)
			{
				GFX::Cmd::BindRenderTarget(context, m_BloomTexturesUpsample[i]);
				GFX::Cmd::BindSRV<PS>(context, m_BloomTexturesUpsample[i+1], 0);
				GFX::Cmd::BindSRV<PS>(context, m_BloomTexturesDownsample[i], 1);
				SetupAndBindBloomInput(context, m_BloomTexturesDownsample[i]);
				GFX::Cmd::DrawFC(context);
			}

			GFX::Cmd::MarkerEnd(context);
		}

		GFX::Cmd::MarkerEnd(context);
	}

	// Tonemapping
	{
		std::vector<std::string> config{};
		config.push_back("TONEMAPPING");

		if (PostprocessSettings.EnableBloom) config.push_back("APPLY_BLOOM");

		GFX::Cmd::MarkerBegin(context, "Tonemapping");
		GFX::Cmd::SetupStaticSamplers<PS>(context);
		GFX::Cmd::BindRenderTarget(context, GetOutputTexture());
		GFX::Cmd::BindShader<PS | VS>(context, m_PostprocessShader, config);
		GFX::Cmd::BindCBV<PS>(context, m_PostprocessingSettingsBuffer, 0);
		GFX::Cmd::BindSRV<PS>(context, hdrRT, 0);
		GFX::Cmd::BindSRV<PS>(context, m_BloomTexturesUpsample[0], 1);
		GFX::Cmd::DrawFC(context);
		GFX::Cmd::MarkerEnd(context);

		Step();
	}

	// TAA
	if (PostprocessSettings.AntialiasingMode == AntiAliasingMode::TAA)
	{
		// TAA History
		GFX::Cmd::CopyToTexture(context, m_TAAHistory[0], m_TAAHistory[1]);
		GFX::Cmd::CopyToTexture(context, GetInputTexture(), m_TAAHistory[0]);

		GFX::Cmd::MarkerBegin(context, "TAA");
		GFX::Cmd::BindRenderTarget(context, GetOutputTexture());
		GFX::Cmd::BindShader<VS | PS>(context, m_PostprocessShader, { "TAA" });
		GFX::Cmd::SetupStaticSamplers<PS>(context);
		GFX::Cmd::BindSRV<PS>(context, m_TAAHistory[0], 0);
		GFX::Cmd::BindSRV<PS>(context, m_TAAHistory[1], 1);
		GFX::Cmd::BindSRV<PS>(context, motionVectorInput, 2);
		GFX::Cmd::DrawFC(context);
		GFX::Cmd::MarkerEnd(context);

		Step();
	}

	GFX::Cmd::MarkerEnd(context);

	return GetInputTexture();
}

void PostprocessingRenderer::ReloadTextureResources(ID3D11DeviceContext* context)
{
	if (m_ResolvedColor.Valid())
	{
		GFX::Storage::Free(m_ResolvedColor);
		GFX::Storage::Free(m_TAAHistory[0]);
		GFX::Storage::Free(m_TAAHistory[1]);
		GFX::Storage::Free(m_PostprocessRT[0]);
		GFX::Storage::Free(m_PostprocessRT[1]);
	}

	const uint32_t size[2] = { AppConfig.WindowWidth, AppConfig.WindowHeight };
	m_ResolvedColor = GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	m_TAAHistory[0] = GFX::CreateTexture(size[0], size[1], RCF_Bind_SRV | RCF_CopyDest);
	m_TAAHistory[1] = GFX::CreateTexture(size[0], size[1], RCF_Bind_SRV | RCF_CopyDest);
	m_PostprocessRT[0] = GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV);
	m_PostprocessRT[1] = GFX::CreateTexture(size[0], size[1], RCF_Bind_RTV | RCF_Bind_SRV);

	// Bloom
	const float aspect = (float)size[0] / size[1];
	uint32_t bloomTexSize[2] = { 512 * aspect, 512 };
	for (uint32_t i = 0; i < BLOOM_NUM_SAMPLES; i++)
	{
		m_BloomTexturesDownsample[i] = GFX::CreateTexture(bloomTexSize[0], bloomTexSize[1], RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_BloomTexturesUpsample[i] = GFX::CreateTexture(bloomTexSize[0], bloomTexSize[1], RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

		bloomTexSize[0] /= 2;
		bloomTexSize[1] /= 2;
	}

	// Camera
	MainSceneGraph.MainCamera.UseJitter = PostprocessSettings.AntialiasingMode == AntiAliasingMode::TAA;
	for (uint32_t i = 0; i < 16; i++)
	{
		MainSceneGraph.MainCamera.Jitter[i] = 2.0f * ((HaltonSequence[i] - Float2{ 0.5f, 0.5f }) / Float2(AppConfig.WindowWidth, AppConfig.WindowHeight));
	}

}

