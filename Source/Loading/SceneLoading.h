#pragma once

#include "Core/SceneGraph.h"

namespace SceneLoading
{
	void LoadEntityInBackground(const std::string& path, Entity& entityOut);
	void LoadEntity(const std::string& path, Entity& entityOut);
}