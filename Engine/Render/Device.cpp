#include "Device.h"

#include "Render/Commands.h"
#include "Render/Context.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/Memory.h"
#include "Render/Resource.h"
#include "Render/Texture.h"
#include "Render/RenderThread.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

Device* Device::s_Instance = nullptr;

Device::Device()
{
}

Device::~Device()
{
}

void Device::InitDevice()
{
#ifdef DEBUG
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		API_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())));
		debugController->EnableDebugLayer();
	}
#endif // DEBUG

	// DXGI Factory
	API_CALL(CreateDXGIFactory1(IID_PPV_ARGS(m_DXGIFactory.GetAddressOf())));

	// Adapter
	ComPtr<IDXGIAdapter1> dxgiAdapter;
	m_DXGIFactory->EnumAdapters1(0, &dxgiAdapter);

	// Handle
	HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(m_Handle.GetAddressOf()));

	// Fallback to WARP device.
	if (FAILED(hardwareResult))
	{
		API_CALL(m_DXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter)));
		API_CALL(D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(m_Handle.GetAddressOf())));
	}

	// Allocator
	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = m_Handle.Get();
	allocatorDesc.pAdapter = dxgiAdapter.Get();
	API_CALL(D3D12MA::CreateAllocator(&allocatorDesc, &m_Allocator));

	// Context
	m_Context = ScopedRef<GraphicsContext>(GFX::Cmd::CreateGraphicsContext());

	// Memory
	m_Memory.SRVHeap = ScopedRef<DescriptorHeapCPU>(new DescriptorHeapCPU{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 20 * 1024u });
	m_Memory.RTVHeap = ScopedRef<DescriptorHeapCPU>(new DescriptorHeapCPU{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256u });
	m_Memory.DSVHeap = ScopedRef<DescriptorHeapCPU>(new DescriptorHeapCPU{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64u });

	// Create swapchain
	DXGI_SWAP_CHAIN_DESC desc;
	desc.BufferDesc.Width = AppConfig.WindowWidth;
	desc.BufferDesc.Height = AppConfig.WindowHeight;
	desc.BufferDesc.RefreshRate.Numerator = 60;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.BufferDesc.Format = SWAPCHAIN_DEFAULT_FORMAT;
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = SWAPCHAIN_BUFFER_COUNT;
	desc.OutputWindow = Window::Get()->GetHandle();
	desc.Windowed = true;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	 
	API_CALL(m_DXGIFactory->CreateSwapChain(m_Context->CmdQueue.Get(), &desc, m_SwapchainHandle.GetAddressOf()));

	// Additional resources
	m_CopyShader = ScopedRef<Shader>(new Shader{ "Engine/Render/copy.hlsl" });

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

	ResourceInitData initData = { m_Context.get(), fcVBData.data()};
	m_QuadBuffer = ScopedRef<Buffer>(GFX::CreateBuffer((uint32_t) fcVBData.size() * sizeof(FCVert), sizeof(FCVert), RCF_None, &initData));
	GFX::SetDebugName(m_QuadBuffer.get(), "Device::QuadBuffer");

	GFX::Cmd::SubmitContext(*m_Context);

	RecreateSwapchain();
}

void Device::DeinitDevice()
{
	DeferredTrash::ClearAll();

	m_CopyShader = nullptr;
	m_QuadBuffer = nullptr;
	m_Context = nullptr;

	for (uint32_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
		m_SwapchainBuffers[i] = nullptr;

	m_SwapchainHandle = nullptr;

	m_DXGIFactory = nullptr;
	m_Allocator = nullptr;
	m_Handle = nullptr;
}

void Device::RecreateSwapchain()
{
	// Wait for GPU to finish before recreating swapchain resources and reset context
	GFX::Cmd::FlushContext(*m_Context);
	GFX::Cmd::ResetContext(*m_Context);

	// Release swapchain resources
	for (uint8_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++) m_SwapchainBuffers[i] = nullptr;

	// Resize/recreate resources
	m_CurrentSwapchainBuffer = 0;
	API_CALL(m_SwapchainHandle->ResizeBuffers(SWAPCHAIN_BUFFER_COUNT, AppConfig.WindowWidth, AppConfig.WindowHeight, SWAPCHAIN_DEFAULT_FORMAT, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	for (uint8_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
	{
		m_SwapchainBuffers[i] = ScopedRef<Texture>(new Texture{});
		m_SwapchainBuffers[i]->CreationFlags = RCF_Bind_RTV;
		API_CALL(m_SwapchainHandle->GetBuffer(i, IID_PPV_ARGS(m_SwapchainBuffers[i]->Handle.GetAddressOf())));
		m_SwapchainBuffers[i]->Width = AppConfig.WindowWidth;
		m_SwapchainBuffers[i]->Height = AppConfig.WindowHeight;
		m_SwapchainBuffers[i]->NumMips = 1;
		m_SwapchainBuffers[i]->CurrState = D3D12_RESOURCE_STATE_COMMON;
		m_SwapchainBuffers[i]->Format = SWAPCHAIN_DEFAULT_FORMAT;
		// m_SwapchainBuffers[i]->RowPitch = TODO
		// m_SwapchainBuffers[i]->SlicePitch = 
		m_SwapchainBuffers[i]->RTV = m_Memory.RTVHeap->Alloc();

		D3D12_RENDER_TARGET_VIEW_DESC rtViewDesc{};
		rtViewDesc.Format = SWAPCHAIN_DEFAULT_FORMAT;
		rtViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtViewDesc.Texture2D.MipSlice = 0;
		rtViewDesc.Texture2D.PlaneSlice = 0;

		m_Handle->CreateRenderTargetView(m_SwapchainBuffers[i]->Handle.Get(), &rtViewDesc, m_SwapchainBuffers[i]->RTV);
	}
}

void Device::EndFrame(Texture* texture)
{
	const CompiledShader& copyShader = GFX::GetCompiledShader(m_CopyShader.get(), {}, VS | PS);
	
	// Copy to swapchain
	GFX::Cmd::MarkerBegin(*m_Context, "Copy to swapchain");
	GraphicsState copyState;
	copyState.Table.SRVs.push_back(texture);
	GFX::Cmd::BindSampler(copyState, 0, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_FILTER_MIN_MAG_MIP_POINT);
	GFX::Cmd::BindRenderTarget(copyState, m_SwapchainBuffers[m_CurrentSwapchainBuffer].get());
	GFX::Cmd::BindShader(copyState, m_CopyShader.get(), VS | PS);
	GFX::Cmd::DrawFC(*m_Context, copyState);
	GFX::Cmd::TransitionResource(*m_Context, m_SwapchainBuffers[m_CurrentSwapchainBuffer].get(), D3D12_RESOURCE_STATE_PRESENT);
	GFX::Cmd::MarkerEnd(*m_Context);

	// Deferred tasks
	GFX::Cmd::MarkerBegin(*m_Context, "Deferred tasks");
	m_TaskExecutor.Run(*m_Context);
	GFX::Cmd::MarkerEnd(*m_Context);

	// Submit context
	GFX::Cmd::SubmitContext(*m_Context);

	// Present
	m_SwapchainHandle->Present(0, 0);
	m_CurrentSwapchainBuffer = (m_CurrentSwapchainBuffer + 1) % SWAPCHAIN_BUFFER_COUNT;
}

DeferredTaskExecutor::~DeferredTaskExecutor()
{
	while (!m_Tasks.empty())
	{
		RenderTask* task = m_Tasks.front();
		m_Tasks.pop();
		delete task;
	}
}

void DeferredTaskExecutor::Run(GraphicsContext& context)
{
	for (uint32_t i = 0; i < m_MaxTasksPerFrame && !m_Tasks.empty(); i++)
	{
		RenderTask* task = m_Tasks.front();
		m_Tasks.pop();
		task->Run(context);
		delete task;
	}
}
