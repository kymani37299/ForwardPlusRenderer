#include "SSAORenderer.h"

#include <Engine/Render/Commands.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Texture.h>
#include <Engine/System/ApplicationConfiguration.h>
#include <Engine/Utility/Random.h>
#include <Engine/Utility/MathUtility.h>

#include "Globals.h"
#include "Scene/SceneGraph.h"
#include "Renderers/Util/ConstantManager.h"
#include "Shaders/shared_definitions.h"

void SSAORenderer::Init(ID3D11DeviceContext* context)
{
	for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; i++)
	{
		Float3 sample = Float3{ Random::SNorm(), Random::SNorm(), Random::UNorm() }.NormalizeFast();
		sample *= Random::UNorm();

		const float scale = (float) i / SSAO_KERNEL_SIZE;
		sample *= MathUtility::Lerp(0.1f, 1.0f, scale * scale);

		m_Kernel.push_back(Float4{ sample.x, sample.y, sample.z, 0.0f });
	}

	std::vector<ColorUNORM> noise;
	for (uint32_t i = 0; i < 16; i++)
	{
		const ColorUNORM noiseValue = { Random::SNorm(), Random::SNorm(), 0.0f, 1.0f };
		noise.push_back(noiseValue);
	}
	m_NoiseTexture = GFX::CreateTexture(4, 4, RCF_Bind_SRV, 1, DXGI_FORMAT_R8G8B8A8_UNORM, noise.data());

	const ColorUNORM defaultColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	m_NoSSAOTexture = GFX::CreateTexture(1, 1, RCF_Bind_SRV, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &defaultColor);

	m_Shader = GFX::CreateShader("Forward+/Shaders/ssao.hlsl");

	UpdateResources(context);
}

struct SSAOSettingsRenderData
{
	float SampleRadius;
	float Power;
	float DepthBias;
	float Padding;
};

SSAOSettingsRenderData GetSettingsRenderData()
{
	SSAOSettings& settings = RenderSettings.SSAO;

	SSAOSettingsRenderData data{};
	data.SampleRadius = settings.SampleRadius;
	data.Power = settings.Power;
	data.DepthBias = settings.DepthBias;

	return data;
}

TextureID SSAORenderer::Draw(ID3D11DeviceContext* context, TextureID depth)
{
	if (!RenderSettings.SSAO.Enabled) return m_NoSSAOTexture;

	GFX::Cmd::MarkerBegin(context, "SSAO");

	// Sample
	{
		CBManager.Clear();
		CBManager.Add(GetSettingsRenderData());
		CBManager.Add(MainSceneGraph.MainCamera.CameraData);
		for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; i++) CBManager.Add(m_Kernel[i]);
		CBManager.Add(AppConfig.WindowWidth);
		CBManager.Add(AppConfig.WindowHeight, true);

		GFX::Cmd::BindRenderTarget(context, m_SSAOSampleTexture);
		GFX::Cmd::BindShader<VS | PS>(context, m_Shader, { "SSAO_SAMPLE" });
		GFX::Cmd::BindSRV<PS>(context, m_NoiseTexture, 0);
		GFX::Cmd::BindSRV<PS>(context, depth, 1);
		GFX::Cmd::DrawFC(context);
	}


	GFX::Cmd::BindRenderTarget(context, TextureID{});

	// Blur
	{
		CBManager.Clear();
		CBManager.Add(AppConfig.WindowWidth);
		CBManager.Add(AppConfig.WindowHeight, true);

		GFX::Cmd::BindRenderTarget(context, m_SSAOTexture);
		GFX::Cmd::BindShader<VS | PS>(context, m_Shader, { "SSAO_BLUR" });
		GFX::Cmd::BindSRV<PS>(context, m_SSAOSampleTexture, 0);
		GFX::Cmd::DrawFC(context);
	}
	

	GFX::Cmd::MarkerEnd(context);

	return m_SSAOTexture;
}

void SSAORenderer::UpdateResources(ID3D11DeviceContext* context)
{
	if (m_SSAOTexture.Valid())
	{
		GFX::Storage::Free(m_SSAOSampleTexture);
		GFX::Storage::Free(m_SSAOTexture);
	}
	m_SSAOSampleTexture = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R16_UNORM);
	m_SSAOTexture = GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV | RCF_Bind_SRV, 1, DXGI_FORMAT_R16_UNORM);
}
