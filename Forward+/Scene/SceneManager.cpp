#include "SceneManager.h"

#include <Engine/Common.h>
#include <Engine/Render/Device.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/RenderThread.h>
#include <Engine/Utility/Random.h>

#include "Scene/SceneLoading.h"
#include "Scene/SceneGraph.h"

SceneManager* SceneManager::s_Instance;

namespace
{
	template<SceneSelection Scene> void LoadSceneSelection(GraphicsContext& context) 
	{
		NOT_IMPLEMENTED;
	}

	template<> void LoadSceneSelection<SceneSelection::SimpleBoxes>(GraphicsContext& context)
	{
		Drawable plane{};
		plane.Position = { 0.0f, -10.0f, 0.0f };
		plane.Scale = { 10000.0f, 1.0f, 10000.0f };
		SceneLoading::LoadedScene cubeScene = SceneLoading::Load(context, "Resources/cube/cube.gltf");
		SceneLoading::AddDraws(context, cubeScene, plane);

		constexpr uint32_t NUM_CUBES = 50;
		for (uint32_t i = 0; i < NUM_CUBES; i++)
		{
			const Float3 position = Float3{ Random::SNorm(), Random::SNorm(), Random::SNorm() } *Float3{ 100.0f, 100.0f, 100.0f };
			const Float3 scale = Float3{ Random::Float(0.1f, 10.0f), Random::Float(0.1f, 10.0f) , Random::Float(0.1f, 10.0f) };
			Drawable cube{};
			cube.Position = position;
			cube.Scale = scale;
			SceneLoading::AddDraws(context, cubeScene, cube);
		}
	}

	// Note: Too big for the github, so using low res scene on repo
	// const std::string SPONZA_PATH = "Resources/SuperSponza/NewSponza_Main_Blender_glTF.gltf"
	const std::string SPONZA_PATH = "Resources/sponza/sponza.gltf";

	template<> void LoadSceneSelection<SceneSelection::Sponza>(GraphicsContext& context)
	{
		SceneLoading::LoadedScene scene = SceneLoading::Load(context, SPONZA_PATH);

		const Float3 startingPosition{ 350.0f, 0.0f, 200.0f };

		Drawable e{};
		e.Position = startingPosition;
		e.Scale = 10.0f * Float3{ 1.0f, 1.0f, 1.0f };
		SceneLoading::AddDraws(context, scene, e);

		SceneManager::Get().GetSceneGraph().MainCamera.NextTransform.Position += startingPosition;
	}

	template<> void LoadSceneSelection<SceneSelection::SponzaX100>(GraphicsContext& context)
	{
		SceneLoading::LoadedScene scene = SceneLoading::Load(context, SPONZA_PATH);

		constexpr uint32_t NUM_CASTLES[2] = { 10, 10 };
		constexpr float CASTLE_OFFSET[2] = { 350.0f, 200.0f };
		for (uint32_t i = 0; i < NUM_CASTLES[0]; i++)
		{
			for (uint32_t j = 0; j < NUM_CASTLES[1]; j++)
			{
				Drawable e{};
				e.Position = { i * CASTLE_OFFSET[0], 0.0f , j * CASTLE_OFFSET[1] };
				e.Scale = 10.0f * Float3{ 1.0f, 1.0f, 1.0f };
				SceneLoading::AddDraws(context, scene, e);
			}
		}
		SceneManager::Get().GetSceneGraph().MainCamera.NextTransform.Position += Float3(2 * CASTLE_OFFSET[0], 0.0f, 2 * CASTLE_OFFSET[1]);
	}
}

SceneManager::~SceneManager()
{
	RenderThreadPool::Get()->FlushAndPauseExecution();
	ContextManager::Get().Flush();

	SAFE_DELETE(m_SceneGraph);
	m_CurrentScene = SceneSelection::None;

	RenderThreadPool::Get()->ResumeExecution();
}

void SceneManager::LoadScene(GraphicsContext& context, SceneSelection scene)
{
	PROFILE_SECTION(context, "SceneManager::LoadScene");

	bool resumeThreadPool = false;
	if (m_CurrentScene != SceneSelection::None)
	{
		resumeThreadPool = true;

		RenderThreadPool::Get()->FlushAndPauseExecution();
		ContextManager::Get().Flush();
	}
	
	m_CurrentScene = scene;

	// Init scene graph
	m_SceneGraph = new SceneGraph{};
	m_SceneGraph->InitRenderData(context);

	// Set default lights
	SceneManager::Get().GetSceneGraph().AmbientLight = Float3(0.05f, 0.05f, 0.2f);
	SceneManager::Get().GetSceneGraph().DirLight.Direction = Float3(-0.2f, -1.0f, -0.2f);
	SceneManager::Get().GetSceneGraph().DirLight.Radiance = Float3(3.7f, 2.0f, 0.9f);

	// Load selected scene
	switch (scene)
	{
	case SceneSelection::None:
		LoadSceneSelection<SceneSelection::None>(context);
		break;
	case SceneSelection::SimpleBoxes:
		LoadSceneSelection<SceneSelection::SimpleBoxes>(context);
		break;
	case SceneSelection::Sponza:
		LoadSceneSelection<SceneSelection::Sponza>(context);
		break;
	case SceneSelection::SponzaX100:
		LoadSceneSelection<SceneSelection::SponzaX100>(context);
		break;
	default:
		NOT_IMPLEMENTED;
		break;
	}

	if (resumeThreadPool)
	{
		RenderThreadPool::Get()->ResumeExecution();
	}
}