#pragma once

#include "Common.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

class Window
{
public:
	static void Init() { s_Instance = new Window(); }
	static Window* Get() { return s_Instance; }
	static void Destroy() { SAFE_DELETE(s_Instance); }

private:
	static Window* s_Instance;

public:
	void Update(float dt);

	void Shutdown() { m_Running = false; }

	bool IsRunning() const { return m_Running; }
	HWND GetHandle() const { return m_Handle; }
	bool IsCursorShown() const { return  m_ShowCursor; }

	void ShowCursor(bool show);
	void AddToTitle(const std::string& title);
private:
	Window();

private:
	bool m_Running = false;
	bool m_ShowCursor = true;

	HWND m_Handle;
};

namespace WindowInput
{
	void InputFrameBegin();
	void InputFrameEnd();

	bool IsKeyPressed(unsigned int key);
	bool IsKeyJustPressed(unsigned int key);
	Float2 GetMousePos();
	Float2 GetMouseDelta();
}