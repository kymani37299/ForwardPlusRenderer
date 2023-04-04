#include "SkyboxRenderer.h"

#include <Engine/Render/Context.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Device.h>
#include <Engine/Render/Texture.h>
#include <Engine/Loading/TextureLoading.h>

#include "Renderers/Util/ConstantBuffer.h"
#include "Scene/SceneGraph.h"

Buffer* GenerateCubeVB(GraphicsContext& context);

static void ProcessAllCubemapFaces(GraphicsContext& context, GraphicsState& faceState, Texture* cubemap)
{
	Buffer* CubeVB = GenerateCubeVB(context);
	
	const auto getCameraForFace = [](uint32_t faceIndex)
	{
		using namespace DirectX;
		static Float3 forwards[6] = { Float3(-1.0f,  0.0f,  0.0f), Float3(1.0f,  0.0f,  0.0f), Float3(0.0f,  -1.0f,  0.0f), Float3(0.0f, 1.0f,  0.0f), Float3(0.0f,  0.0f,  1.0f), Float3(0.0f,  0.0f, -1.0f) };
		static Float3 ups[6] = { Float3(0.0f, -1.0f,  0.0f), Float3(0.0f, -1.0f,  0.0f), Float3(0.0f,  0.0f,  -1.0f), Float3(0.0f,  0.0f, 1.0f), Float3(0.0f, -1.0f,  0.0f), Float3(0.0f, -1.0f,  0.0f) };

		XMMATRIX viewMat = XMMatrixLookAtLH(Float3(0.0f, 0.0f, 0.0f).ToXM(), forwards[faceIndex].ToXM(), ups[faceIndex].ToXM());
		XMMATRIX projMat = XMMatrixOrthographicLH(2.0f, 2.0f, 0.1f, 10.0f);
		XMMATRIX viewProj = XMMatrixMultiply(viewMat, projMat);
		return XMUtility::ToHLSLFloat4x4(viewProj);
	};

	faceState.VertexBuffers[0] = CubeVB;
	
	DescriptorAllocation oldRTV = cubemap->RTV;

	for (uint32_t i = 0; i < 6; i++)
	{
		DescriptorHeap* heap = Device::Get()->GetMemory().RTVHeap.get();
		DescriptorAllocation rtvDescriptor = heap->Allocate();
		
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = cubemap->Format;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.ArraySize = 1;
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		rtvDesc.Texture2DArray.MipSlice = 0;
		Device::Get()->GetHandle()->CreateRenderTargetView(cubemap->Handle.Get(), &rtvDesc, rtvDescriptor.GetCPUHandle());

		DirectX::XMFLOAT4X4 worldToClip = getCameraForFace(i);
		ConstantBuffer cb{};
		cb.Add(worldToClip);
		faceState.Table.CBVs[0] = cb.GetBuffer(context);
		cubemap->RTV = rtvDescriptor;
		faceState.RenderTargets[0] = cubemap;
		context.ApplyState(faceState);
		GFX::Cmd::Draw(context, CubeVB->ByteSize / CubeVB->Stride, 0);

		GFX::Cmd::Delete(context, rtvDescriptor);
	}

	cubemap->RTV = oldRTV;

	GFX::Cmd::Delete(context, CubeVB);
}

static Texture* PanoramaToCubemap(GraphicsContext& context, Texture* panoramaTexture, uint32_t cubemapSize)
{
	PROFILE_SECTION(context, "Panorama to cubemap");

	Texture* cubemapTex = GFX::CreateTextureArray(cubemapSize, cubemapSize, 6, RCF::RTV | RCF::Cubemap);

	Shader* shader = new Shader("Forward+/Shaders/quadrilateral2cubemap.hlsl");
	
	GraphicsState state;
	state.Shader = shader;
	state.Table.SRVs[0] = panoramaTexture;
	state.Table.SMPs[0] = { D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_WRAP };

	ProcessAllCubemapFaces(context, state, cubemapTex);
	GFX::Cmd::Delete(context, shader);

	return cubemapTex;
}

static Texture* CubemapToIrradianceMap(GraphicsContext& context, Texture* cubemapTexture, uint32_t cubemapSize)
{
	PROFILE_SECTION(context, "Calculate irradiance");

	Texture* cubemapTex = GFX::CreateTextureArray(cubemapSize, cubemapSize, 6, RCF::RTV | RCF::Cubemap, 1, DXGI_FORMAT_R11G11B10_FLOAT);
	Shader* shader = new Shader("Forward+/Shaders/calculate_irradiance.hlsl");
	
	GraphicsState state;
	state.Shader = shader;

	state.Table.SRVs[0] = cubemapTexture;
	state.Table.SMPs[0] = { D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_WRAP };
	ProcessAllCubemapFaces(context, state, cubemapTex);
	GFX::Cmd::Delete(context, shader);
	
	return cubemapTex;
}

static Texture* GenerateSkybox(GraphicsContext& context, const std::string& texturePath)
{
	Texture* skyboxPanorama = TextureLoading::LoadTextureHDR(context, texturePath, RCF::None);
	Texture* cubemap = PanoramaToCubemap(context, skyboxPanorama, SkyboxRenderer::CUBEMAP_SIZE);
	GFX::Cmd::Delete(context, skyboxPanorama);
	return cubemap;
}

void SkyboxRenderer::Init(GraphicsContext& context)
{
	m_SkyboxCubemap = ScopedRef<Texture>(GenerateSkybox(context, m_SkyboxTexturePath));
	m_IrradianceCubemap = ScopedRef<Texture>(CubemapToIrradianceMap(context, m_SkyboxCubemap.get(), CUBEMAP_SIZE));
	m_SkyboxShader = ScopedRef<Shader>(new Shader("Forward+/Shaders/skybox.hlsl"));
	m_CubeVB = ScopedRef<Buffer>(GenerateCubeVB(context));

	GFX::SetDebugName(m_SkyboxCubemap.get(), "SkyboxRenderer::SkyboxCubemap");
	GFX::SetDebugName(m_IrradianceCubemap.get(), "SkyboxRenderer::IrradianceCubemap");
	GFX::SetDebugName(m_CubeVB.get(), "SkyboxRenderer::CubeVB");
}

void SkyboxRenderer::Draw(GraphicsContext& context, GraphicsState& state)
{
	PROFILE_SECTION(context, "Skybox");

	state.StencilRef = 0xff;
	state.DepthStencilState.StencilEnable = true;
	state.DepthStencilState.StencilWriteMask = 0;
	state.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	state.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	state.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	state.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
	state.DepthStencilState.BackFace = state.DepthStencilState.FrontFace;

	ConstantBuffer cb{};
	cb.Add(SceneManager::Get().GetSceneGraph().MainCamera.CameraData);

	state.Table.CBVs[0] = cb.GetBuffer(context);
	state.Table.SRVs[0] = m_SkyboxCubemap.get();
	state.Table.SMPs[0] = { D3D12_FILTER_MIN_MAG_MIP_LINEAR , D3D12_TEXTURE_ADDRESS_MODE_WRAP };
	state.VertexBuffers[0] = m_CubeVB.get();
	state.Shader = m_SkyboxShader.get();

	context.ApplyState(state);
	GFX::Cmd::Draw(context, m_CubeVB->ByteSize/m_CubeVB->Stride, 0);
}

void SkyboxRenderer::OnShaderReload(GraphicsContext& context)
{
	// GfxRef<Texture> tmpRef = GfxRef<Texture>(m_SkyboxCubemap.get());
	// m_SkyboxCubemap = ScopedRef<Texture>(GenerateSkybox(context, m_SkyboxTexturePath));
	// GFX::SetDebugName(m_SkyboxCubemap.get(), "SkyboxRenderer::SkyboxCubemap");

	// Too long, enable if needed
	// m_IrradianceCubemap = ScopedRef<Texture>(CubemapToIrradianceMap(context, m_SkyboxCubemap.get(), CUBEMAP_SIZE));
}
