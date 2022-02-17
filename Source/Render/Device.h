#pragma once

#include <vector>

#include "Common.h"

#include "Render/RenderAPI.h"
#include "Render/ResourceID.h"
#include "Utility/Multithreading.h"

class Device
{
public:
	static void Init() { s_Instance = new Device(); s_Instance->DeferredInit(); }
	static Device* Get() { return s_Instance; }
	static void Destroy() 
	{
		ID3D11Device* device = s_Instance->m_Device;
		SAFE_DELETE(s_Instance);

#ifdef DEBUG
		ID3D11Debug* d3dDebug = nullptr;
		device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug);
		d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_IGNORE_INTERNAL | D3D11_RLDO_DETAIL);
#endif
		device->Release();
	}

private:
	static Device* s_Instance;

public:
	Device();
	~Device();

	void EndFrame(TextureID finalImage);
	void CreateSwapchain();

	ID3D11Device* GetHandle() const { return m_Device; }
	ID3D11DeviceContext1* GetContext() const { return m_Context.Get(); }

	void SubmitDeferredContext(ID3D11DeviceContext* context);

	std::vector<ID3D11SamplerState*>& GetStaticSamplers() { return m_StaticSamplers; }
private:
	// Initialize components that will use Device::Get()
	void DeferredInit();
	void CreateStaticSamplers();
private:
	ID3D11Device* m_Device;
	ComPtr<ID3D11DeviceContext1> m_Context;

	ComPtr<IDXGISwapChain1> m_Swapchain;
	ComPtr<ID3D11Texture2D> m_SwapchainTexture;
	ComPtr<ID3D11RenderTargetView> m_SwapchainView;

	std::vector<ID3D11SamplerState*> m_StaticSamplers;

	ShaderID m_CopyShader;
	BufferID m_QuadBuffer;

	MTR::MutexVector<ID3D11CommandList*> m_PendingCommandLists;
};