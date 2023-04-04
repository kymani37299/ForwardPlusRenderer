#pragma once

#include <vector>
#include <unordered_map>

#include "Common.h"

struct GraphicsContext;
struct Texture;

DEFINE_ENUM_CLASS_FLAGS(GUIFlags);
enum class GUIFlags : uint32_t
{
	None = 0,
	ShowOnStart = 1, // Always set open it on start of application
	MenuHidden = 2,  // Not visible in menus, must be triggered in code
	OnlyButton = 4, // 
};

class GUIElement
{
public:
	GUIElement(const std::string name, GUIFlags flags = GUIFlags::None):
		m_Name(name), 
		m_Shown(TestFlag(flags, GUIFlags::ShowOnStart)),
		m_Flags(flags)
	{}

	virtual ~GUIElement() {}

	virtual void Update(float dt) {};
	void RenderElement(GraphicsContext& context);

	virtual void Reset() {}
	virtual void OnMenuButtonPress() {}

	std::string GetName() const { return m_Name; }
	bool& GetShownRef() { return m_Shown; }

	GUIFlags GetFlags() const { return m_Flags; }

protected:
	virtual void Render(GraphicsContext& context) {};

private:
	std::string m_Name = "";
	bool m_Shown = false;
	GUIFlags m_Flags;
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