#include <Windows.h>
#include <iostream>

#include "Common.h"
#include "Core/Renderer.h"
#include "Core/SceneGraph.h"
#include "Render/Commands.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/Device.h"
#include "Render/PipelineState.h"
#include "Loading/SceneLoading.h"
#include "System/VSConsoleRedirect.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

ApplicationConfiguration AppConfig;

class AppScope
{
public:
	AppScope()
	{
		Window::Init();
	}

	~AppScope()
	{
		Window::Destroy();
	}
};

SceneGraph MainSceneGraph;

class GeometryRenderPass : public RenderPass
{
public:
	void OnInit(ID3D11DeviceContext1* context) override
	{
		MainSceneGraph.Entities.push_back(SceneLoading::LoadEntity("Resources/sponza/sponza.gltf"));
		m_Shader = GFX::CreateShader("Source/Shaders/geometry.hlsl");

		D3D11_SAMPLER_DESC linearBorderDesc{};
		linearBorderDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		linearBorderDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		linearBorderDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		linearBorderDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		Device::Get()->GetHandle()->CreateSamplerState(&linearBorderDesc, m_LinearClampSampler.GetAddressOf());
	}

	void OnDraw(ID3D11DeviceContext1* context) override
	{
		GFX::Cmd::BindShader(context, m_Shader, true);

		for (Entity e : MainSceneGraph.Entities)
		{
			for (Drawable d : e.Drawables)
			{
				Mesh& m = d.Mesh;
				GFX::Cmd::BindVertexBuffers(context, { m.Position, m.UV, m.Normal, m.Tangent });
				GFX::Cmd::BindIndexBuffer(context, m.Indices);
				GFX::Cmd::BindTextureSRV(context, d.Material.Albedo, 0);
				context->PSSetSamplers(0, 1, m_LinearClampSampler.GetAddressOf());
				context->DrawIndexed(GFX::GetNumBufferElements(m.Indices), 0, 0);
			}
		}
	}

private:
	ShaderID m_Shader;
	ComPtr<ID3D11SamplerState> m_LinearClampSampler;
};

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParams, int showFlags)
{
	AppConfig.AppHandle = instance;
	AppConfig.WindowTitle = "Forward plus graphics";
	AppConfig.WindowWidth = 1024;
	AppConfig.WindowHeight = 768;

	AppScope _appScope;
	RedirectToVSConsoleScoped _vsConsoleRedirect;

	Renderer r;
	r.AddRenderPass(new GeometryRenderPass());
	float dt = 0.0f;

	while (Window::Get()->IsRunning())
	{
		Window::Get()->Update(dt);
		r.Update(dt);
		r.Render();
	}
}