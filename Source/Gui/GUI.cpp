#include "GUI.h"

#include <Windows.h>

#include "Render/Commands.h"
#include "Render/Device.h"
#include "GUI/ImGui/imgui.h"
#include "GUI/ImGui/imgui_impl_dx11.h"
#include "GUI/ImGui/imgui_impl_win32.h"
#include "System/ApplicationConfiguration.h"
#include "System/Window.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

GUI* GUI::s_Instance = nullptr;

void GUIElement::RenderElement(ID3D11DeviceContext* context)
{
	if (m_Shown)
	{
		if (ImGui::Begin(m_Name.c_str(), &m_Shown))
		{
			Render(context);
			ImGui::End();
		}
	}
	
}

GUI::GUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(Window::Get()->GetHandle());
	ImGui_ImplDX11_Init(Device::Get()->GetHandle(), Device::Get()->GetContext());
}

GUI::~GUI()
{
	ImGui_ImplDX11_Shutdown();
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

void GUI::Render(ID3D11DeviceContext* context)
{
	if (!m_Visible) return;

	GFX::Cmd::MarkerBegin(context, "ImGUI");
	
	ImGui_ImplDX11_NewFrame();
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
		element->RenderElement(context);
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	GFX::Cmd::MarkerEnd(context);
}

void GUI::Reset()
{
	for (GUIElement* element : m_Elements)
	{
		element->Reset();
	}
}