#include "Window.h"

#include <unordered_map>
#include <debugapi.h>
#include <string>

#include "System/ApplicationConfiguration.h"
#include "Utility/StringUtility.h"

Window* Window::s_Instance = nullptr;

namespace
{
    RECT GetClipRect()
    {
        RECT rc{};
        ASSERT(GetWindowRect(Window::Get()->GetHandle(), &rc), "[WinApi] GetWindowRect failed!");
        long width = rc.right - rc.left;
        long height = rc.bottom - rc.top;
        rc.left += width / 4;
        rc.right -= width / 4;
        rc.top += height / 4;
        rc.bottom -= height / 4;
        return rc;
    }
}

namespace WindowInput
{
    static Float2 s_MousePos = {0.0f, 0.0f};
    static Float2 s_MouseDelta = { 0.0f, 0.0f };

    static std::unordered_map<unsigned int, bool> s_InputDownMap;
    static std::unordered_map<unsigned int, bool> s_BeginFrameInputs;
    static std::unordered_map<unsigned int, bool> s_JustPressedInputMap;

    void InputFrameBegin()
    {
        // Just pressed inputs
        static std::unordered_map<unsigned int, bool>::iterator it;
        for (it = s_InputDownMap.begin(); it != s_InputDownMap.end(); it++)
        {
            s_BeginFrameInputs[it->first] = it->second;
        }
    }

    void InputFrameEnd()
    {
        static Window* wnd = Window::Get();

        // Just pressed inputs
        static std::unordered_map<unsigned int, bool>::iterator it;
        for (it = s_InputDownMap.begin(); it != s_InputDownMap.end(); it++)
        {
            const unsigned int key = it->first;
            const bool hasElement = s_BeginFrameInputs.find(key) == s_BeginFrameInputs.end();
            s_JustPressedInputMap[it->first] = (hasElement || s_BeginFrameInputs[key] != s_InputDownMap[key]) && s_InputDownMap[key];
        }

        // Mouse
        if (wnd->GetHandle() == GetActiveWindow())
        {
            POINT cursorPos{};
            ASSERT(GetCursorPos(&cursorPos), "[WinApi] GetCursorPos failed!");
            RECT wr{};
            ASSERT(GetWindowRect(Window::Get()->GetHandle(), &wr), "[WinApi] GetWindowRect failed!");

            const Float2 mousePosNormalized = Float2(
                ((float)(cursorPos.x - wr.left) / AppConfig.WindowWidth),
                ((float)(cursorPos.y - wr.top) / AppConfig.WindowHeight));
            s_MouseDelta = mousePosNormalized - s_MousePos;

            if (wnd->IsCursorShown())
            {
                s_MousePos = mousePosNormalized;
            }
            else
            {
                const RECT rc = GetClipRect();
                const long rcCenterX = (rc.right - rc.left) / 2;
                const long rcCenterY = (rc.bottom - rc.top) / 2;

                s_MousePos = Float2((float)rcCenterX / AppConfig.WindowWidth, (float)rcCenterY / AppConfig.WindowHeight);
                ASSERT(SetCursorPos(rcCenterX, rcCenterY), "[WinApi] SetCursorPos failed!");
            }
        }
    }

    void OnKeyStateChanged(unsigned int key, unsigned int state)
    {
        s_InputDownMap[key] = state == WM_KEYDOWN;
    }

    bool IsKeyPressed(unsigned int key)
    {
        if (s_InputDownMap.find(key) == s_InputDownMap.end()) return false;
        return s_InputDownMap[key];
    }

    bool IsKeyJustPressed(unsigned int key)
    {
        if (s_JustPressedInputMap.find(key) == s_JustPressedInputMap.end()) return false;
        return s_JustPressedInputMap[key];
    }

    Float2 GetMousePos()
    {
        return s_MousePos;
    }

    Float2 GetMouseDelta()
    {
        return s_MouseDelta;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (msg)
    {
    case WM_SIZE:
    {
        AppConfig.WindowWidth = LOWORD(lparam);
        AppConfig.WindowHeight = HIWORD(lparam);
        AppConfig.WindowSizeDirty = true;
        break;
    }
    case WM_KEYUP:
    case WM_KEYDOWN:
    {
        WindowInput::OnKeyStateChanged((uint32_t)wparam, msg);
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    default:
        result = DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return result;
}

Window::Window()
{
    WNDCLASSEXW winClass = {};
    winClass.cbSize = sizeof(WNDCLASSEXW);
    winClass.style = CS_HREDRAW | CS_VREDRAW;
    winClass.lpfnWndProc = &WndProc;
    winClass.hInstance = static_cast<HINSTANCE>(AppConfig.AppHandle);
    winClass.hIcon = LoadIconW(0, IDI_APPLICATION);
    winClass.hCursor = LoadCursorW(0, IDC_ARROW);
    winClass.lpszClassName = L"MyWindowClass";
    winClass.hIconSm = LoadIconW(0, IDI_APPLICATION);

    if (!RegisterClassExW(&winClass)) {
        MessageBoxA(0, "RegisterClassEx failed", "Fatal Error", MB_OK);
        return;
    }

    RECT initialRect = { 0, 0, (long) AppConfig.WindowWidth, (long) AppConfig.WindowHeight };
    AdjustWindowRectEx(&initialRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW);
    const LONG initialWidth = initialRect.right - initialRect.left;
    const LONG initialHeight = initialRect.bottom - initialRect.top;
    const std::wstring wTitle = StringUtility::ToWideString(AppConfig.WindowTitle);

    m_Handle = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
        winClass.lpszClassName,
        wTitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        initialWidth,
        initialHeight,
        0, 0, static_cast<HINSTANCE>(AppConfig.AppHandle), 0);

    if (!m_Handle) {
        MessageBoxA(0, "CreateWindowEx failed", "Fatal Error", MB_OK);
        return;
    }

    m_Running = true;
}

void Window::Update(float dt)
{
    MSG message = {};
    while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
            m_Running = false;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void Window::ShowCursor(bool show)
{
    ::ShowCursor(show);
    m_ShowCursor = show;
}

void Window::AddToTitle(const std::string& title)
{
    const std::string fullTitle = AppConfig.WindowTitle + title;
    SetWindowTextA(m_Handle, fullTitle.c_str());
}