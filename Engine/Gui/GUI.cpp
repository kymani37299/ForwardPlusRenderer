#include "GUI.h"

#include <Windows.h>

#include "Render/Device.h"
#include "Render/Context.h"
#include "Render/Commands.h"
#include "Render/Texture.h"
#include "GUI/ImGui_Core.h"
#include "GUI/ImGui/imgui_impl_dx12.h"
#include "GUI/ImGui/imgui_impl_win32.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

GUI* GUI::s_Instance = nullptr;

void GUIElement::RenderElement(GraphicsContext& context)
{
	if (TestFlag(GetFlags(), GUIFlags::OnlyButton))
		return;

	if (m_Shown)
	{
		if (ImGui::Begin(m_Name.c_str(), &m_Shown))
		{
			Render(context);
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
	PROFILE_SECTION_CPU("GUI::Update");
	for (auto& it : m_Elements)
	{
		for (GUIElement* element : it.second)
		{
			element->Update(dt);
		}
	}
}

void GUI::Render(GraphicsContext& context)
{
	PROFILE_SECTION(context, "GUI::Render");

	if (!m_Initialized)
	{
		// NOTE: If we are having multiple contexts make sure that context that initialized ImGui is used for imgui draw
		Device* device = Device::Get();
		DeviceMemory& deviceMemory = device->GetMemory();
		DescriptorHeap& srvHeap = *deviceMemory.SRVHeapGPU;
		DescriptorAllocation alloc = srvHeap.Allocate(256u);
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = alloc.GetCPUHandle();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = alloc.GetGPUHandle();
		ImGui_ImplDX12_Init(Device::Get()->GetHandle(), Device::SWAPCHAIN_BUFFER_COUNT, Device::SWAPCHAIN_DEFAULT_FORMAT, srvHeap.GetHeap(), cpuHandle, gpuHandle);

		m_Initialized = true;
	}

	if (!m_Visible || m_Elements.empty()) return;

	// Prepare imgui context
	{
		// Bind Descriptor heaps
		ID3D12DescriptorHeap* descriptorHeaps[] = { Device::Get()->GetMemory().SRVHeapGPU->GetHeap()};
		context.CmdList->SetDescriptorHeaps(1, descriptorHeaps);

		// Bind render target
		Device::Get()->BindSwapchainToRenderTarget(context);
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
					if (!TestFlag(element->GetFlags(), GUIFlags::MenuHidden))
					{
						if (ImGui::MenuItem(element->GetName().c_str(), 0, &element->GetShownRef()))
						{
							element->OnMenuButtonPress();
						}
					}
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
			element->RenderElement(context);
			ImGui::PopID();
			id++;
		}
	}

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), context.CmdList.Get());
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