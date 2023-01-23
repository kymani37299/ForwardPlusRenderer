#pragma once

// Hack for compiling imgui
#ifndef IMGUI_HACK
#define IMGUI_HACK
#ifdef WIN32
#define ImTextureID ImU64
#endif // WIN32
#endif // IMGUI_HACK

#include "Common.h"
#include "Gui/Imgui/imgui.h"

namespace ImGui
{
	inline bool DragUint(const std::string& label, uint32_t& value, const float stepSize = 1.0f)
	{
		int intValue = value;
		if (ImGui::DragInt(label.c_str(), &intValue, stepSize))
		{
			value = intValue;
			return true;
		}
		return false;
	}

	inline bool DragFloat(const std::string& label, Float4& value, const float stepSize = 0.1f)
	{
		float tmpValues[4];
		tmpValues[0] = value.x;
		tmpValues[1] = value.y;
		tmpValues[2] = value.z;
		tmpValues[3] = value.w;

		if (ImGui::DragFloat4(label.c_str(), tmpValues, stepSize))
		{
			value = Float4{ tmpValues[0], tmpValues[1], tmpValues[2], tmpValues[3] };
			return true;
		}
		return false;
	}

	inline bool DragFloat(const std::string& label, Float3& value, const float stepSize = 0.1f)
	{
		float tmpValues[3];
		tmpValues[0] = value.x;
		tmpValues[1] = value.y;
		tmpValues[2] = value.z;

		if (ImGui::DragFloat3(label.c_str(), tmpValues, stepSize))
		{
			value = Float3{ tmpValues[0], tmpValues[1], tmpValues[2] };
			return true;
		}
		return false;
	}

	inline bool DragFloat(const std::string& label, Float2& value, const float stepSize = 0.1f)
	{
		float tmpValues[2];
		tmpValues[0] = value.x;
		tmpValues[1] = value.y;

		if (ImGui::DragFloat2(label.c_str(), tmpValues, stepSize))
		{
			value = Float2{ tmpValues[0], tmpValues[1] };
			return true;
		}
		return false;
	}

	inline bool DragFloat(const std::string& label, DirectX::XMFLOAT4& value, const float stepSize = 0.1f)
	{
		Float4 tmp{ value };
		if (ImGui::DragFloat(label, tmp, stepSize))
		{
			value = tmp.ToXMFA();
			return true;
		}
		return false;
	}

	inline bool DragFloat(const std::string& label, DirectX::XMFLOAT3A& value, const float stepSize = 0.1f)
	{
		Float3 tmp{ value };
		if (ImGui::DragFloat(label, tmp, stepSize))
		{
			value = tmp.ToXMFA();
			return true;
		}
		return false;
	}

	inline bool DragFloat(const std::string& label, DirectX::XMFLOAT2& value, const float stepSize = 0.1f)
	{
		Float2 tmp{ value };
		if (ImGui::DragFloat(label, tmp, stepSize))
		{
			value = tmp.ToXMF();
			return true;
		}
		return false;
	}

	inline bool ColorEdit(const std::string& label, DirectX::XMFLOAT3A& value)
	{
		float tmpValues[3];
		tmpValues[0] = value.x;
		tmpValues[1] = value.y;
		tmpValues[2] = value.z;

		if (ImGui::ColorEdit3(label.c_str(), tmpValues))
		{
			value.x = tmpValues[0];
			value.y = tmpValues[1];
			value.z = tmpValues[2];
			return true;
		}
		return false;
	}
}