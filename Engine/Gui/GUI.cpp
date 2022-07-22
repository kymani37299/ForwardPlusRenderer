#include "GUI.h"

#include <Windows.h>

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
	for (GUIElement* element : m_Elements)
	{
		element->Update(dt);
	}
}

void GUI::Render(GraphicsContext& context, Texture* renderTarget)
{
	if (!m_Visible) return;

	if (!m_Initialized)
	{
		// NOTE: If we are having multiple contexts make sure that context that initialized ImGui is used for imgui draw
		Device* device = Device::Get();
		MemoryContext& memContext = device->GetContext().MemContext;
		DescriptorHeapGPU& srvHeap = memContext.SRVHeap;
		DescriptorHeapGPU::Allocation alloc = srvHeap.Alloc(memContext.SRVPersistentPage);
		ImGui_ImplDX12_Init(device->GetHandle(), Device::SWAPCHAIN_BUFFER_COUNT, renderTarget->Format, srvHeap.Heap.Get(), alloc.CPUHandle, alloc.GPUHandle);

		m_Initialized = true;
	}

	GFX::Cmd::MarkerBegin(context, "ImGUI");

	// Prepare imgui context
	{
		// Bind Descriptor heaps
		ID3D12DescriptorHeap* descriptorHeaps[] = { context.MemContext.SRVHeap.Heap.Get() };
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
		if (ImGui::BeginMenu("Tools"))
		{
			for (GUIElement* element : m_Elements)
			{
				ImGui::MenuItem(element->GetName().c_str(), 0, &element->GetShownRef());
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// Draw elements
	for (GUIElement* element : m_Elements)
	{
		element->RenderElement();
	}

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), context.CmdList.Get());

	GFX::Cmd::MarkerEnd(context);
}

void GUI::Reset()
{
	for (GUIElement* element : m_Elements)
	{
		element->Reset();
	}
}