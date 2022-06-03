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

void PostprocessingRenderer::Init(ID3D11DeviceContext* context)
{
	m_PostprocessShader = GFX::CreateShader("Source/Shaders/postprocessing.hlsl");
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

	// Tonemapping
	{
		std::vector<std::string> config{};
		config.push_back("TONEMAPPING");
		if (PostprocessSettings.UseExposureTonemapping) config.push_back("EXPOSURE");
		else config.push_back("REINHARD");

		GFX::Cmd::MarkerBegin(context, "Tonemapping");
		GFX::Cmd::SetupStaticSamplers<PS>(context);
		GFX::Cmd::BindRenderTarget(context, GetOutputTexture());
		GFX::Cmd::BindShader<PS | VS>(context, m_PostprocessShader, config);
		GFX::Cmd::BindCBV<PS>(context, m_PostprocessingSettingsBuffer, 0);
		GFX::Cmd::BindSRV<PS>(context, hdrRT, 0);
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

	MainSceneGraph.MainCamera.UseJitter = PostprocessSettings.AntialiasingMode == AntiAliasingMode::TAA;
	for (uint32_t i = 0; i < 16; i++)
	{
		MainSceneGraph.MainCamera.Jitter[i] = 2.0f * ((HaltonSequence[i] - Float2{ 0.5f, 0.5f }) / Float2(AppConfig.WindowWidth, AppConfig.WindowHeight));
	}

}

