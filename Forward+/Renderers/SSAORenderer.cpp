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
#include "Renderers/Util/ConstantBuffer.h"
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
	m_NoiseTexture = ScopedRef<Texture>(GFX::CreateTexture(4, 4, RCF::None, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &noiseData));

	const ColorUNORM defaultColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	ResourceInitData defaultData{ &context, &defaultColor };
	m_NoSSAOTexture = ScopedRef<Texture>(GFX::CreateTexture(1, 1, RCF::None, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &defaultData));

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

	PROFILE_SECTION(context, "SSAO");

	// Sample
	{
		GraphicsState state{};

		ConstantBuffer cb{};
		cb.Add(GetSettingsRenderData());
		cb.Add(SceneManager::Get().GetSceneGraph().MainCamera.CameraData);
		for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; i++) cb.Add(m_Kernel[i]);
		cb.Add(AppConfig.WindowWidth);
		cb.Add(AppConfig.WindowHeight);
		
		state.Table.CBVs[0] = cb.GetBuffer(context);
		state.Table.SRVs[0] = m_NoiseTexture.get();
		state.Table.SRVs[1] = depth;
		state.Table.SMPs[0] = { D3D12_FILTER_MIN_MAG_MIP_POINT , D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
		state.RenderTargets[0] = m_SSAOSampleTexture.get();
		state.Shader = m_Shader.get();
		state.ShaderConfig = { "SSAO_SAMPLE" };
		GFX::Cmd::DrawFC(context, state);
	}

	// Blur
	{
		GraphicsState state{};

		ConstantBuffer cb{};
		cb.Add(AppConfig.WindowWidth);
		cb.Add(AppConfig.WindowHeight);

		state.Table.SMPs[0] = { D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_BORDER, {0.0f, 0.0f, 0.0f, 0.0f} };
		state.Table.CBVs[0] = cb.GetBuffer(context);
		state.Table.SRVs[0] = m_SSAOSampleTexture.get();
		state.RenderTargets[0] = m_SSAOTexture.get();
		state.Shader = m_Shader.get();
		state.ShaderConfig = { "SSAO_BLUR" };
		GFX::Cmd::DrawFC(context, state);
	}

	return m_SSAOTexture.get();
}

void SSAORenderer::UpdateResources(GraphicsContext& context)
{
	m_SSAOSampleTexture = ScopedRef<Texture>(GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF::RTV, 1, DXGI_FORMAT_R16_UNORM));
	m_SSAOTexture = ScopedRef<Texture>(GFX::CreateTexture(AppConfig.WindowWidth, AppConfig.WindowHeight, RCF::RTV, 1, DXGI_FORMAT_R16_UNORM));

	GFX::SetDebugName(m_SSAOSampleTexture.get(), "SSAORenderer::SSAOSample");
	GFX::SetDebugName(m_SSAOSampleTexture.get(), "SSAORenderer::SSAO");
}
