#include "Device.h"

#include "Render/Commands.h"
#include "Render/Context.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Render/DescriptorHeap.h"
#include "Render/Resource.h"
#include "Render/Texture.h"
#include "Render/RenderThread.h"
#include "Render/RenderResources.h"
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

	// Specification
	D3D12_FEATURE_DATA_D3D12_OPTIONS1 features1{};
	m_Handle->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &features1, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS1));
	m_Specification.SupportWaveIntrinscs = features1.WaveOps;
	m_Specification.WavefrontSize = features1.WaveLaneCountMin;

	// Allocator
	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = m_Handle.Get();
	allocatorDesc.pAdapter = dxgiAdapter.Get();
	API_CALL(D3D12MA::CreateAllocator(&allocatorDesc, &m_Allocator));

	// Command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	API_CALL(m_Handle->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_CommandQueue.GetAddressOf())));

	// Profiling
	ID3D12CommandQueue* cmdQueues[] = { m_CommandQueue.Get() };
	uint32_t numQueues = STATIC_ARRAY_SIZE(cmdQueues);
	OPTICK_GPU_INIT_D3D12(m_Handle.Get(), cmdQueues, numQueues);

	// Context
	ContextManager::Get().Init();
	GraphicsContext& context = ContextManager::Get().GetCreationContext();
	GFX::Cmd::BeginRecording(context);

	// Memory
	m_Memory.SRVHeap = ScopedRef<DescriptorHeap>(new DescriptorHeap{false,  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 20 * 1024u });
	m_Memory.RTVHeap = ScopedRef<DescriptorHeap>(new DescriptorHeap{false,  D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256u });
	m_Memory.DSVHeap = ScopedRef<DescriptorHeap>(new DescriptorHeap{false,  D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64u });
	m_Memory.SMPHeap = ScopedRef<DescriptorHeap>(new DescriptorHeap{false,  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256u });
	m_Memory.SRVHeapGPU = ScopedRef<DescriptorHeap>(new DescriptorHeap{ true, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 64u * 1024, 256u * 1024u });
	m_Memory.SMPHeapGPU = ScopedRef<DescriptorHeap>(new DescriptorHeap{ true, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256u,  1024u });

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
	 
	API_CALL(m_DXGIFactory->CreateSwapChain(m_CommandQueue.Get(), &desc, m_SwapchainHandle.GetAddressOf()));

	GFX::Cmd::EndRecordingAndSubmit(context);

	RecreateSwapchain(context);
}

void Device::DeinitDevice()
{
	ContextManager::Get().Destroy();

	for (uint32_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
		m_SwapchainBuffers[i] = nullptr;

	m_SwapchainHandle = nullptr;

	m_DXGIFactory = nullptr;
	m_Allocator = nullptr;
	m_Handle = nullptr;
}

void Device::RecreateSwapchain(GraphicsContext& context)
{
	PROFILE_SECTION(context, "RecreateSwapchain");

	// Wait for GPU to finish before recreating swapchain resources
	ContextManager::Get().Flush();

	// Release swapchain resources
	for (uint8_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++) m_SwapchainBuffers[i] = nullptr;

	// Resize/recreate resources
	m_CurrentSwapchainBuffer = 0;
	API_CALL(m_SwapchainHandle->ResizeBuffers(SWAPCHAIN_BUFFER_COUNT, AppConfig.WindowWidth, AppConfig.WindowHeight, SWAPCHAIN_DEFAULT_FORMAT, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	for (uint8_t i = 0; i < SWAPCHAIN_BUFFER_COUNT; i++)
	{
		m_SwapchainBuffers[i] = ScopedRef<Texture>(new Texture{});
		m_SwapchainBuffers[i]->CreationFlags = RCF::RTV;
		API_CALL(m_SwapchainHandle->GetBuffer(i, IID_PPV_ARGS(m_SwapchainBuffers[i]->Handle.GetAddressOf())));
		m_SwapchainBuffers[i]->Width = AppConfig.WindowWidth;
		m_SwapchainBuffers[i]->Height = AppConfig.WindowHeight;
		m_SwapchainBuffers[i]->NumMips = 1;
		m_SwapchainBuffers[i]->CurrState = D3D12_RESOURCE_STATE_COMMON;
		m_SwapchainBuffers[i]->Format = SWAPCHAIN_DEFAULT_FORMAT;
		// m_SwapchainBuffers[i]->RowPitch = TODO
		// m_SwapchainBuffers[i]->SlicePitch = 
		m_SwapchainBuffers[i]->RTV = m_Memory.RTVHeap->Allocate();

		D3D12_RENDER_TARGET_VIEW_DESC rtViewDesc{};
		rtViewDesc.Format = SWAPCHAIN_DEFAULT_FORMAT;
		rtViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtViewDesc.Texture2D.MipSlice = 0;
		rtViewDesc.Texture2D.PlaneSlice = 0;

		m_Handle->CreateRenderTargetView(m_SwapchainBuffers[i]->Handle.Get(), &rtViewDesc, m_SwapchainBuffers[i]->RTV.GetCPUHandle());
	}
}

void Device::BindSwapchainToRenderTarget(GraphicsContext& context)
{
	Texture* swapchain = m_SwapchainBuffers[m_CurrentSwapchainBuffer].get();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = swapchain->RTV.GetCPUHandle();

	GFX::Cmd::TransitionResource(context, swapchain, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)swapchain->Width, (float)swapchain->Height, 0.0f, 1.0f };
	D3D12_RECT scissor = { 0,0, (long)swapchain->Width, (long)swapchain->Height };
	context.CmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
	context.CmdList->RSSetViewports(1, &viewport);
	context.CmdList->RSSetScissorRects(1, &scissor);
}

void Device::CopyToSwapchain(GraphicsContext& context, Texture* texture)
{
	PROFILE_SECTION(context, "Copy to swapchain");

	GraphicsState copyState;
	copyState.Table.SRVs[0] = texture;
	copyState.RenderTargets[0] = m_SwapchainBuffers[m_CurrentSwapchainBuffer].get();
	copyState.Shader = GFX::RenderResources.CopyShader.get();
	copyState.Table.SMPs[0] = Sampler{ D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
	GFX::Cmd::DrawFC(context, copyState);
}

void Device::EndFrame(GraphicsContext& context)
{
	PROFILE_SECTION(context, "EndFrame");
	{
		PROFILE_SECTION(context, "Deferred tasks");
		m_TaskExecutor.Run(context);
	}

	// Submit context
	GFX::Cmd::TransitionResource(context, m_SwapchainBuffers[m_CurrentSwapchainBuffer].get(), D3D12_RESOURCE_STATE_PRESENT);
	GFX::Cmd::EndRecordingAndSubmit(context);
	
	// Profiling
	OPTICK_GPU_FLIP(m_SwapchainHandle.Get());
	OPTICK_CATEGORY("Present", Optick::Category::Wait);

	// Present
	m_SwapchainHandle->Present(AppConfig.VSyncEnabled ? 1 : 0, 0);
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
