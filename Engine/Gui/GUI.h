#pragma once

#include <vector>
#include <unordered_map>

#include "Common.h"

struct GraphicsContext;
struct Texture;

class GUIElement
{
public:
	GUIElement(const std::string name, bool shown): m_Name(name), m_Shown(shown) {}
	virtual ~GUIElement() {}

	virtual void Reset() {}
	virtual void Update(float dt) = 0;
	void RenderElement();

	std::string GetName() const { return m_Name; }
	bool& GetShownRef() { return m_Shown; }

protected:
	virtual void Render() = 0;

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
	inline void PushMenu(const std::string& menuName) { m_CurrentMenu = menuName; }
	inline void PopMenu() { m_CurrentMenu = "Menu"; }

	inline void AddElement(GUIElement* element) { m_Elements[m_CurrentMenu].push_back(element); }
	inline void RemoveMenu(const std::string& menuName)
	{
		if (!m_Elements.contains(menuName)) return;

		for (GUIElement* el : m_Elements[menuName])
		{
			delete el;
		}
		m_Elements.erase(menuName);
	}

	bool HandleWndProc(void* hwnd, unsigned int msg, unsigned int wparam, long lparam);

	void Update(float dt);
	void Render(GraphicsContext& context);
	void Reset();

	inline void SetVisible(bool value) { m_Visible = value; }
	inline bool ToggleVisible() { return m_Visible = !m_Visible; }

private:
	bool m_Initialized = false;
	bool m_Visible = true;

	std::string m_CurrentMenu = "Menu";
	std::unordered_map<std::string, std::vector<GUIElement*>> m_Elements;
};