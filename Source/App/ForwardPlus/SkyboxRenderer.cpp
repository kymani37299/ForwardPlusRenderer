#include "SkyboxRenderer.h"

#include "App/ForwardPlus/ConstantManager.h"
#include "Core/SceneGraph.h"
#include "Render/Commands.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/Device.h"
#include "Render/Texture.h"

BufferID GenerateCubeVB();

static void ProcessAllCubemapFaces(ID3D11DeviceContext* context, TextureID cubemap)
{
	// TODO: Use CBManager
	static BufferID CameraCB;
	static BufferID CubeVB;
	if (!CubeVB.Valid())
	{
		CameraCB = GFX::CreateConstantBuffer<DirectX::XMFLOAT4X4>();
		CubeVB = GenerateCubeVB();
	}

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

	const Texture& cubemapTexHandle = GFX::Storage::GetTexture(cubemap);

	GFX::Cmd::BindVertexBuffer(context, CubeVB);
	GFX::Cmd::BindCBV<VS>(context, CameraCB, 0);

	for (uint32_t i = 0; i < 6; i++)
	{
		// Create RTV
		ID3D11RenderTargetView* rtv;
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = cubemapTexHandle.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.ArraySize = 1;
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		rtvDesc.Texture2DArray.MipSlice = 0;
		API_CALL(Device::Get()->GetHandle()->CreateRenderTargetView(cubemapTexHandle.Handle.Get(), &rtvDesc, &rtv));

		DirectX::XMFLOAT4X4 worldToClip = getCameraForFace(i);
		GFX::Cmd::UploadToBuffer(context, CameraCB, 0, &worldToClip, 0, sizeof(DirectX::XMFLOAT4X4));

		context->OMSetRenderTargets(1, &rtv, nullptr);
		context->Draw(GFX::GetNumElements(CubeVB), 0);

		rtv->Release();
	}

	GFX::Cmd::BindRenderTarget(context, TextureID{});
}

static TextureID PanoramaToCubemap(ID3D11DeviceContext* context, TextureID panoramaTexture, uint32_t cubemapSize)
{
	TextureID cubemapTex = GFX::CreateTextureArray(cubemapSize, cubemapSize, 6, RCF_Bind_SRV | RCF_Bind_RTV | RCF_Cubemap);
	ShaderID shader = GFX::CreateShader("Source/Shaders/quadrilateral2cubemap.hlsl");

	GFX::Cmd::MarkerBegin(context, "Panorama to cubemap");
	GFX::Cmd::BindShader<VS | PS>(context, shader);
	GFX::Cmd::BindSRV<PS>(context, panoramaTexture, 0);
	GFX::Cmd::SetupStaticSamplers<PS>(context);
	GFX::Cmd::SetViewport(context, cubemapSize, cubemapSize);
	ProcessAllCubemapFaces(context, cubemapTex);
	GFX::Storage::Free(shader);
	GFX::Cmd::MarkerEnd(context);

	return cubemapTex;
}

static TextureID CubemapToIrradianceMap(ID3D11DeviceContext* context, TextureID cubemapTexture, uint32_t cubemapSize)
{
	TextureID cubemapTex = GFX::CreateTextureArray(cubemapSize, cubemapSize, 6, RCF_Bind_SRV | RCF_Bind_RTV | RCF_Cubemap, 1, DXGI_FORMAT_R11G11B10_FLOAT);
	ShaderID shader = GFX::CreateShader("Source/Shaders/calculate_irradiance.hlsl");

	GFX::Cmd::MarkerBegin(context, "Calculate irradiance");
	GFX::Cmd::BindShader<VS | PS>(context, shader);
	GFX::Cmd::BindSRV<PS>(context, cubemapTexture, 0);
	GFX::Cmd::SetupStaticSamplers<PS>(context);
	GFX::Cmd::SetViewport(context, cubemapSize, cubemapSize);
	ProcessAllCubemapFaces(context, cubemapTex);
	GFX::Storage::Free(shader);
	GFX::Cmd::MarkerEnd(context);

	return cubemapTex;
}

static TextureID GenerateSkybox(ID3D11DeviceContext* context, const std::string& texturePath)
{
	TextureID skyboxPanorama = GFX::LoadTextureHDR(texturePath, RCF_Bind_SRV);
	TextureID cubemap = PanoramaToCubemap(context, skyboxPanorama, 512);
	GFX::Storage::Free(skyboxPanorama);
	return cubemap;
}

void SkyboxRenderer::Init(ID3D11DeviceContext* context)
{
	m_SkyboxCubemap = GenerateSkybox(context, m_SkyboxTexturePath);
	m_IrradianceCubemap = CubemapToIrradianceMap(context, m_SkyboxCubemap, 512);
	m_SkyboxShader = GFX::CreateShader("Source/Shaders/skybox.hlsl");
	m_CubeVB = GenerateCubeVB();
}

void SkyboxRenderer::Draw(ID3D11DeviceContext* context)
{
	PipelineState pso = GFX::DefaultPipelineState();
	pso.DS.StencilEnable = true;
	pso.DS.StencilWriteMask = 0;
	pso.DS.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	pso.DS.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	pso.DS.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	pso.DS.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
	pso.DS.BackFace = pso.DS.FrontFace;

	GFX::Cmd::MarkerBegin(context, "Skybox");

	CBManager.Clear();
	CBManager.Add(MainSceneGraph.MainCamera.CameraData, true);

	GFX::Cmd::SetPipelineState(context, pso);
	
	GFX::Cmd::BindShader<VS | PS>(context, m_SkyboxShader);
	GFX::Cmd::BindVertexBuffer(context, m_CubeVB);
	GFX::Cmd::BindSRV<PS>(context, m_SkyboxCubemap, 0);
	GFX::Cmd::SetupStaticSamplers<PS>(context);
	context->Draw(GFX::GetNumElements(m_CubeVB), 0);
	GFX::Cmd::MarkerEnd(context);
}

void SkyboxRenderer::OnShaderReload(ID3D11DeviceContext* context)
{
	GFX::Storage::Free(m_SkyboxCubemap);
	m_SkyboxCubemap = GenerateSkybox(context, m_SkyboxTexturePath);
}
