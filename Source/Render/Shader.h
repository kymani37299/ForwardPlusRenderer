#pragma once

#include <vector>

#include "Render/ResourceID.h"

struct ShaderImplementation;

namespace GFX
{
	ShaderID CreateShader(const std::string& path, uint32_t creationFlags = SCF_PS | SCF_VS);
	const ShaderImplementation& GetShaderImplementation(ShaderID shaderID, const std::vector<std::string>& defines);
}