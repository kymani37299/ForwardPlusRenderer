#pragma once

#include <vector>

#include "Render/ResourceID.h"

namespace GFX
{
	ShaderID CreateShader(const std::string& path, const std::vector<std::string>& defines = {}, uint32_t creationFlags = SCF_PS | SCF_VS);
	bool ReloadShader(ShaderID shaderID);
}