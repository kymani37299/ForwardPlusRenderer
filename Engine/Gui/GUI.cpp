#include "GUI.h"

#include <Windows.h>

// Hack for compiling imgui
#ifdef WIN32
#define ImTextureID ImU64
#endif // WIN32

#include "Render/Device.h"
#include "Render/Memory.h"
#include "Render/Context.h"
#include "Render/Commands.h"
#include "Render/Texture.h"
#include "GUI/ImGui/imgui.h"
#include "GUI/ImGui/imgui_impl_dx12.h"
#include "GUI/ImGui/imgui_impl_win32.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

GUI* GUI::s_Instance = nullptr;

void GUIElement::RenderElement()
{
	if (m_Shown)
	{
		if (ImGui::Begin(m_Name.c_str(), &m_Shown))
		{
			Render();
		}
		ImGui::End();
	}
	
}

GUI::GUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(Window::Get()->GetHandle());
}

GUI::~GUI()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

bool GUI::HandleWndProc(void* hwnd, unsigned int msg, unsigned int wparam, long lparam)
{
	return ImGui_ImplWin32_WndProcHandler((HWND)hwnd, msg, wparam, lparam);
}

void GUI::Update(float dt)
{
	for (auto& it : m_Elements)
	{
		for (GUIElement* element : it.second)
		{
			element->Update(dt);
		}
	}
}

void GUI::Render(GraphicsContext& context, Texture* renderTarget)
{
	if (!m_Initialized)
	{
		// NOTE: If we are having multiple contexts make sure that context that initialized ImGui is used for imgui draw
		Device* device = Device::Get();
		MemoryContext& memContext = device->GetContext().MemContext;
		DescriptorHeapGPU& srvHeap = memContext.SRVHeap;
		DescriptorAllocation alloc = memContext.SRVHeap.Allocate(256u);
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = memContext.SRVHeap.GetCPUHandle(alloc);
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = memContext.SRVHeap.GetGPUHandle(alloc);
		ImGui_ImplDX12_Init(device->GetHandle(), Device::SWAPCHAIN_BUFFER_COUNT, renderTarget->Format, srvHeap.GetHeap(), cpuHandle, gpuHandle);

		m_Initialized = true;
	}

	if (!m_Visible || m_Elements.empty()) return;

	GFX::Cmd::MarkerBegin(context, "ImGUI");

	// Prepare imgui context
	{
		// Bind Descriptor heaps
		ID3D12DescriptorHeap* descriptorHeaps[] = { context.MemContext.SRVHeap.GetHeap() };
		context.CmdList->SetDescriptorHeaps(1, descriptorHeaps);

		// Bind render target
		GFX::Cmd::TransitionResource(context, renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)renderTarget->Width, (float)renderTarget->Height, 0.0f, 1.0f };
		D3D12_RECT scissor = { 0,0, (long)renderTarget->Width, (long)renderTarget->Height };
		context.CmdList->OMSetRenderTargets(1, &renderTarget->RTV, false, nullptr);
		context.CmdList->RSSetViewports(1, &viewport);
		context.CmdList->RSSetScissorRects(1, &scissor);
	}

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Draw menu bar
	if (ImGui::BeginMainMenuBar())
	{
		for (auto& it : m_Elements) 
		{
			if (ImGui::BeginMenu(it.first.c_str()))
			{
				for (GUIElement* element : it.second)
				{
					ImGui::MenuItem(element->GetName().c_str(), 0, &element->GetShownRef());
				}
				ImGui::EndMenu();
			}
		}
		ImGui::EndMainMenuBar();
	}

	// Draw elements
	uint32_t id = 0;
	for (auto& it : m_Elements)
	{
		for (GUIElement* element : it.second)
		{
			ImGui::PushID(id);
			element->RenderElement();
			ImGui::PopID();
			id++;
		}
	}

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), context.CmdList.Get());

	GFX::Cmd::MarkerEnd(context);
}

void GUI::Reset()
{
	for (auto& it : m_Elements)
	{
		for (GUIElement* element : it.second)
		{
			element->Reset();
		}
	}
}