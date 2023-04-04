#pragma once

struct GraphicsContext;
struct SceneGraph;

enum class SceneSelection
{
	None,
	SimpleBoxes,
	Sponza,
	SponzaX100,

	Count,
};

class SceneManager
{
private:
	static SceneManager* s_Instance;

public:
	static SceneManager& Get()
	{
		if (!s_Instance)
		{
			s_Instance = new SceneManager{};
		}
		return *s_Instance;
	}

	static void Destroy()
	{
		if (s_Instance)
		{
			delete s_Instance;
		}
	}

public:
	~SceneManager();

	void LoadScene(GraphicsContext& context, SceneSelection scene);
	
	SceneGraph& GetSceneGraph() { return *m_SceneGraph; }
	SceneSelection GetCurrentScene() const { return m_CurrentScene; }

private:
	SceneSelection m_CurrentScene = SceneSelection::None;

	SceneGraph* m_SceneGraph = nullptr;
};