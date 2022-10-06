#pragma once

#include "Common.h"
#include "Loading/ModelLoading.h"

namespace AnimationOperations
{
	enum class AnimationType
	{
		Once,
		Repeat,
		PingPong
	};

	DirectX::XMFLOAT4X4 GetAnimationTransformation(const std::vector<ModelLoading::AnimationEntry>& animations, float t, AnimationType animationType);
	std::vector<float> GetAnimatedMorphWeights(const ModelLoading::SceneObject& object, float t, AnimationType animationType);
}