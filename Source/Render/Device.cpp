#include "Device.h"

#include <vector>

#include "Render/Shader.h"
#include "Render/Buffer.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

#ifdef DEBUG
#include <dxgidebug.h>
#endif // DEBUG

Device* Device::s_Instance = nullptr;

Device::Device()
{
    ID3D11Device* baseDevice;
    ID3D11DeviceContext* baseDeviceContext;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE,
        0, creationFlags,
        featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &baseDevice,
        0, &baseDeviceContext);

    if (FAILED(hr)) {
        std::cout << "FATAL ERROR: D3D11CreateDevice() failed" << std::endl;
        FORCE_CRASH;
    }

    API_CALL(baseDevice->QueryInterface(IID_PPV_ARGS(&m_Device)));
    baseDevice->Release();

    API_CALL(baseDeviceContext->QueryInterface(IID_PPV_ARGS(m_Context.GetAddressOf())));
    baseDeviceContext->Release();
}

void Device::DeferredInit()
{
    CreateSwapchain();

    struct FCVert
    {
        Float2 Position;
        Float2 UV;
    };

    const std::vector<FCVert> fcVBData = {
        FCVert{	{1.0,1.0},		{1.0,0.0}},
        FCVert{	{-1.0,-1.0},	{0.0,1.0}},
        FCVert{	{1.0,-1.0},		{1.0,1.0}},
        FCVert{	{1.0,1.0},		{1.0,0.0}},
        FCVert{ {-1.0,1.0},		{0.0,0.0}},
        FCVert{	{-1.0,-1.0},	{0.0,1.0}}
    };

    m_CopyShader = GFX::CreateShader("Source/Render/Shaders/copy.hlsl");
    m_QuadBuffer = GFX::CreateVertexBuffer(fcVBData.size() * sizeof(FCVert), sizeof(FCVert), fcVBData.data());
}

void Device::Present(RenderTargetID finalRT)
{
    const Texture& colorTex = GFX::Storage::GetTexture(finalRT.ColorTexture);
    const Shader& shader = GFX::Storage::GetShader(m_CopyShader);
    const Buffer& buffer = GFX::Storage::GetBuffer(m_QuadBuffer);
    m_Context->VSSetShader(shader.VS.Get(), nullptr, 0);
    m_Context->PSSetShader(shader.PS.Get(), nullptr, 0);
    m_Context->IASetInputLayout(shader.IL.Get());

    ID3D11ShaderResourceView* srv = colorTex.SRV.Get();
    uint32_t offsets = 0;
    uint32_t strides = buffer.ElementStride;
    ID3D11Buffer* dxBuffer = buffer.Handle.Get();
    m_Context->IASetVertexBuffers(0, 1, &dxBuffer, &strides, &offsets);
    m_Context->PSSetShaderResources(0, 1, &srv);
    m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)AppConfig.WindowWidth, (float)AppConfig.WindowHeight, 0.0f, 1.0f };
    m_Context->RSSetViewports(1, &viewport);
    m_Context->OMSetRenderTargets(1, m_SwapchainView.GetAddressOf(), nullptr);
    m_Context->Draw(6, 0);

    m_Swapchain->Present(AppConfig.VSyncEnabled ? 1 : 0, 0);
}

void Device::CreateSwapchain()
{
    m_SwapchainView.Reset();
    m_SwapchainTexture.Reset();
    m_Swapchain.Reset();

    IDXGIFactory2* dxgiFactory;
    {
        IDXGIDevice1* dxgiDevice;
        API_CALL(m_Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDevice));

        IDXGIAdapter* dxgiAdapter;
        API_CALL(dxgiDevice->GetAdapter(&dxgiAdapter));
        dxgiDevice->Release();

        DXGI_ADAPTER_DESC adapterDesc;
        dxgiAdapter->GetDesc(&adapterDesc);

        API_CALL(dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory));
        dxgiAdapter->Release();
    }

    DXGI_SWAP_CHAIN_DESC1 d3d11SwapChainDesc = {};
    d3d11SwapChainDesc.Width = AppConfig.WindowWidth;
    d3d11SwapChainDesc.Height = AppConfig.WindowHeight;
    d3d11SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    d3d11SwapChainDesc.SampleDesc.Count = 1;
    d3d11SwapChainDesc.SampleDesc.Quality = 0;
    d3d11SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d3d11SwapChainDesc.BufferCount = 2;
    d3d11SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    d3d11SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    d3d11SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    d3d11SwapChainDesc.Flags = 0;

    API_CALL(dxgiFactory->CreateSwapChainForHwnd(m_Device, Window::Get()->GetHandle(), &d3d11SwapChainDesc, 0, 0, m_Swapchain.GetAddressOf()));

    dxgiFactory->Release();

    API_CALL(m_Swapchain->GetBuffer(0, IID_PPV_ARGS(m_SwapchainTexture.GetAddressOf())));
    API_CALL(m_Device->CreateRenderTargetView(m_SwapchainTexture.Get(), 0, m_SwapchainView.GetAddressOf()));
}