#include "SkyboxRenderer.h"

#include <Engine/Render/Context.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Shader.h>
#include <Engine/Render/Device.h>
#include <Engine/Render/Texture.h>
#include <Engine/Render/Memory.h>
#include <Engine/Loading/TextureLoading.h>

#include "Renderers/Util/ConstantManager.h"
#include "Renderers/Util/SamplerManager.h"
#include "Scene/SceneGraph.h"

Buffer* GenerateCubeVB(GraphicsContext& context);

static void ProcessAllCubemapFaces(GraphicsContext& context, GraphicsState& faceState, Texture* cubemap)
{
	Buffer* CubeVB = GenerateCubeVB(context);
	DeferredTrash::Put(CubeVB);

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

	faceState.VertexBuffers.push_back(CubeVB);
	faceState.Table.CBVs.resize(1);
	faceState.RenderTargets.resize(1);

	D3D12_CPU_DESCRIPTOR_HANDLE oldRTV = cubemap->RTV;

	for (uint32_t i = 0; i < 6; i++)
	{
		DescriptorHeapCPU* heap = Device::Get()->GetMemory().RTVHeap.get();
		D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptor = heap->Allocate();
		DeferredTrash::Put(heap, rtvDescriptor);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = cubemap->Format;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.ArraySize = 1;
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		rtvDesc.Texture2DArray.MipSlice = 0;
		Device::Get()->GetHandle()->CreateRenderTargetView(cubemap->Handle.Get(), &rtvDesc, rtvDescriptor);

		DirectX::XMFLOAT4X4 worldToClip = getCameraForFace(i);
		CBManager.Clear();
		CBManager.Add(worldToClip);
		faceState.Table.CBVs[0] = CBManager.GetBuffer();
		cubemap->RTV = rtvDescriptor;
		faceState.RenderTargets[0] = cubemap;
		GFX::Cmd::BindState(context, faceState);
		context.CmdList->DrawInstanced(CubeVB->ByteSize / CubeVB->Stride, 1, 0, 0);
	}

	cubemap->RTV = oldRTV;
}

static Texture* PanoramaToCubemap(GraphicsContext& context, Texture* panoramaTexture, uint32_t cubemapSize)
{
	Texture* cubemapTex = GFX::CreateTextureArray(cubemapSize, cubemapSize, 6, RCF_Bind_RTV | RCF_Cubemap);

	Shader* shader = new Shader("Forward+/Shaders/quadrilateral2cubemap.hlsl");
	DeferredTrash::Put(shader);

	GraphicsState state;
	state.Shader = shader;

	GFX::Cmd::MarkerBegin(context, "Panorama to cubemap");
	state.Table.SRVs.push_back(panoramaTexture);
	SSManager.Bind(state);
	ProcessAllCubemapFaces(context, state, cubemapTex);
	GFX::Cmd::MarkerEnd(context);

	return cubemapTex;
}

static Texture* CubemapToIrradianceMap(GraphicsContext& context, Texture* cubemapTexture, uint32_t cubemapSize)
{
	Texture* cubemapTex = GFX::CreateTextureArray(cubemapSize, cubemapSize, 6, RCF_Bind_RTV | RCF_Cubemap, 1, DXGI_FORMAT_R11G11B10_FLOAT);

	Shader* shader = new Shader("Forward+/Shaders/calculate_irradiance.hlsl");
	DeferredTrash::Put(shader);

	GraphicsState state;
	state.Shader = shader;

	GFX::Cmd::MarkerBegin(context, "Calculate irradiance");
	state.Table.SRVs.push_back(cubemapTexture);
	SSManager.Bind(state);
	ProcessAllCubemapFaces(context, state, cubemapTex);
	GFX::Cmd::MarkerEnd(context);

	return cubemapTex;
}

static Texture* GenerateSkybox(GraphicsContext& context, const std::string& texturePath)
{
	Texture* skyboxPanorama = TextureLoading::LoadTextureHDR(texturePath, RCF_None);
	DeferredTrash::Put(skyboxPanorama);

	Texture* cubemap = PanoramaToCubemap(context, skyboxPanorama, SkyboxRenderer::CUBEMAP_SIZE);
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
	state.StencilRef = 0xff;
	state.DepthStencilState.StencilEnable = true;
	state.DepthStencilState.StencilWriteMask = 0;
	state.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	state.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	state.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	state.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
	state.DepthStencilState.BackFace = state.DepthStencilState.FrontFace;

	GFX::Cmd::MarkerBegin(context, "Skybox");

	CBManager.Clear();
	CBManager.Add(MainSceneGraph->MainCamera.CameraData);
	state.Table.CBVs.push_back(CBManager.GetBuffer());
	state.Table.SRVs.push_back(m_SkyboxCubemap.get());
	state.VertexBuffers.push_back(m_CubeVB.get());
	SSManager.Bind(state);
	state.Shader = m_SkyboxShader.get();

	GFX::Cmd::BindState(context, state);
	context.CmdList->DrawInstanced(m_CubeVB->ByteSize/m_CubeVB->Stride, 1, 0, 0);
	GFX::Cmd::MarkerEnd(context);
}

void SkyboxRenderer::OnShaderReload(GraphicsContext& context)
{
	// GfxRef<Texture> tmpRef = GfxRef<Texture>(m_SkyboxCubemap.get());
	// m_SkyboxCubemap = ScopedRef<Texture>(GenerateSkybox(context, m_SkyboxTexturePath));
	// GFX::SetDebugName(m_SkyboxCubemap.get(), "SkyboxRenderer::SkyboxCubemap");

	// Too long, enable if needed
	// m_IrradianceCubemap = ScopedRef<Texture>(CubemapToIrradianceMap(context, m_SkyboxCubemap.get(), CUBEMAP_SIZE));
}
