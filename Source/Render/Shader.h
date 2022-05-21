#pragma once

#include <vector>

#include "Render/ResourceID.h"

struct ShaderImplementation;

namespace GFX
{
	ShaderID CreateShader(const std::string& path);
	const ShaderImplementation& GetShaderImplementation(ShaderID shaderID, const std::vector<std::string>& defines, uint32_t shaderStages);
}