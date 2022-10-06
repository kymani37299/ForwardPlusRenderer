#include "SSAORenderer.h"

#include <Engine/Render/Commands.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Texture.h>
#include <Engine/Render/Buffer.h>
#include <Engine/System/ApplicationConfiguration.h>
#include <Engine/Utility/Random.h>
#include <Engine/Utility/MathUtility.h>

#include "Globals.h"
#include "Scene/SceneGraph.h"
#include "Renderers/Util/ConstantManager.h"
#include "Renderers/Util/SamplerManager.h"
#include "Shaders/shared_definitions.h"

SSAORenderer::SSAORenderer()
{

}

SSAORenderer::~SSAORenderer()
{

}

void SSAORenderer::Init(GraphicsContext& context)
{
	for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; i++)
	{
		Float3 sample = Float3{ Random::SNorm(), Random::SNorm(), Random::UNorm() }.NormalizeFast();
		sample *= Random::UNorm();

		const float scale = (float)i / SSAO_KERNEL_SIZE;
		sample *= MathUtility::Lerp(0.1f, 1.0f, scale * scale);

		m_Kernel.push_back(Float4{ sample.x, sample.y, sample.z, 0.0f });
	}

	std::vector<ColorUNORM> noise;
	for (uint32_t i = 0; i < 16; i++)
	{
		const ColorUNORM noiseValue = { Random::SNorm(), Random::SNorm(), 0.0f, 1.0f };
		noise.push_back(noiseValue);
	}

	ResourceInitData noiseData{ &context, noise.data() };
	m_NoiseTexture = ScopedRef<Texture>(GFX::CreateTexture(4, 4, RCF_None, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &noiseData));

	const ColorUNORM defaultColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	ResourceInitData defaultData{ &context, &defaultColor };
	m_NoSSAOTexture = ScopedRef<Texture>(GFX::CreateTexture(1, 1, RCF_None, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &defaultData));

	m_Shader = ScopedRef<Shader>(new Shader("Forward+/Shaders/ssao.hlsl"));

	UpdateResources(context);

	GFX::SetDebugName(m_NoiseTexture.get(), "SSAORenderer::Noise");
	GFX::SetDebugName(m_NoSSAOTexture.get(), "SSAORenderer::NoSSAO");
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

Texture* SSAORenderer::Draw(GraphicsContext& context, Texture* depth)
{
	if (!RenderSettings.SSAO.Enabled) return m_NoSSAOTexture.get();

	GFX::Cmd::MarkerBegin(context, "SSAO");

	// Sample
	{
		GraphicsState state{};

		CBManager.Clear();
		CBManager.Add(GetSettingsRenderData());
		CBManager.Add(MainSceneGraph->MainCamera.CameraData);
		for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; i++) CBManager.Add(m_Kernel[i]);
		CBManager.Add(AppConfig.WindowWidth);
		CBManager.Add(AppConfig.WindowHeight);
		
		SSManager.Bind(state);

		state.Table.CBVs.push_back(CBManager.GetBuffer());
		state.Table.SRVs.push_back(m_NoiseTexture.get());
		state.Table.SRVs.push_back(depth);
		state.RenderTargets.push_back(m_SSAOSampleTexture.get());
		state.Shader = m_Shader.get();
		state.ShaderConfig = { "SSAO_SAMPLE" };
		GFX::Cmd::DrawFC(context, state);
	}

	// Blur
	{
		GraphicsState state{};

		CBManager.Clear();
		CBManager.Add(AppConfig.WindowWidth);
		CBManager.Add(AppConfig.WindowHeight);

		SSManager.Bind(state);

		state.Table.CBVs.push_back(CBManager.GetBuffer());
		state.Table.SRVs.push_back(m_SSAOSampleTexture.get());
		state.RenderTargets.push_back(m_SSAOTexture.get());
		state.Shader = m_Shader.get();
		state.ShaderConfig = { "SSAO_BLUR" };
		GFX::Cmd::DrawFC(context, state);
	}


	GFX::Cmd::MarkerEnd(context);

	return m_SSAOTexture.get();
}

void SSAORenderer::UpdateResources(GraphicsContext& context)
{
	m_SSAOSampleTexture = ScopedRef<Texture>(GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV, 1, DXGI_FORMAT_R16_UNORM));
	m_SSAOTexture = ScopedRef<Texture>(GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF_Bind_RTV, 1, DXGI_FORMAT_R16_UNORM));

	GFX::SetDebugName(m_SSAOSampleTexture.get(), "SSAORenderer::SSAOSample");
	GFX::SetDebugName(m_SSAOSampleTexture.get(), "SSAORenderer::SSAO");
}
