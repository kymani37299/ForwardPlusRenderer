#pragma once

#include <vector>

#include "Common.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

class GUIElement
{
public:
	GUIElement(const std::string name): m_Name(name) {}
	virtual ~GUIElement() {}

	virtual void Reset() {}
	virtual void Update(float dt) = 0;
	void RenderElement(ID3D11DeviceContext* context);

	std::string GetName() const { return m_Name; }
	bool& GetShownRef() { return m_Shown; }

protected:
	virtual void Render(ID3D11DeviceContext* context) = 0;

private:
	std::string m_Name = "";
	bool m_Shown = false;
};

class GUI
{
public:
	static void Init() { s_Instance = new GUI(); }
	static GUI* Get() { return s_Instance; }
	static void Destroy() { SAFE_DELETE(s_Instance); }

private:
	static GUI* s_Instance;

	GUI();
	~GUI();

public:
	inline void AddElement(GUIElement* element) { m_Elements.push_back(element); }

	bool HandleWndProc(void* hwnd, unsigned int msg, unsigned int wparam, long lparam);

	void Update(float dt);
	void Render(ID3D11DeviceContext* context);
	void Reset();

	inline void SetVisible(bool value) { m_Visible = value; }
	inline bool ToggleVisible() { return m_Visible = !m_Visible; }

private:
	bool m_Visible = true;
	std::vector<GUIElement*> m_Elements;
};