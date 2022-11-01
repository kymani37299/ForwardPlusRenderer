#include "AnimationOperations.h"

namespace AnimationOperations
{
	template<typename T>
	static T Interpolate(const T& a, const T& b, float t, ModelLoading::AnimInterpolation interpolation)
	{
		using namespace DirectX;

		T value{};
		switch (interpolation)
		{
		case ModelLoading::AnimInterpolation::Lerp:
			value = MathUtility::Lerp(a, b, t);
			break;
		case ModelLoading::AnimInterpolation::SLerp:
			value = MathUtility::Slerp(a, b, t);
			break;
		case ModelLoading::AnimInterpolation::Step:
			value = a;
			break;
		case ModelLoading::AnimInterpolation::Cubic:
		case ModelLoading::AnimInterpolation::Invalid:
		default:
			NOT_IMPLEMENTED;
		}
		return value;
	}

	static DirectX::XMMATRIX GetFrameTransform(const ModelLoading::AnimKeyFrame& keyFrameA, const ModelLoading::AnimKeyFrame& keyFrameB, float animationTime,
		ModelLoading::AnimInterpolation interpolationType, ModelLoading::AnimTarget targetType)
	{
		using namespace DirectX;

		XMMATRIX transformation = XMMatrixIdentity();

		switch (targetType)
		{
		case ModelLoading::AnimTarget::Translation: 
			transformation = XMMatrixTranslationFromVector(Interpolate(keyFrameA.Translation, keyFrameB.Translation, animationTime, interpolationType));
			break;
		case ModelLoading::AnimTarget::Rotation:	
			transformation = XMMatrixRotationQuaternion(Interpolate(keyFrameA.Rotation, keyFrameB.Rotation, animationTime, interpolationType));
			break;
		case ModelLoading::AnimTarget::Scale:	
			transformation = XMMatrixScalingFromVector(Interpolate(keyFrameA.Scale, keyFrameB.Scale, animationTime, interpolationType));
			break;
		case ModelLoading::AnimTarget::Weights:
		case ModelLoading::AnimTarget::Invalid:
		default:
			NOT_IMPLEMENTED;
			break;
		}
		return transformation;
	}

	static float floatMod(float a, float b)
	{
		const uint32_t div = (uint32_t) (a / b);
		return a - div * b;
	}

	static void CalculateTargetKeyframes(const ModelLoading::AnimationEntry& entry, float t, ModelLoading::AnimKeyFrame& kfA, ModelLoading::AnimKeyFrame& kfB, float& animationTime, AnimationType animationType)
	{
		float frameTime = 0.0f;
		switch (animationType)
		{
		case AnimationType::Once:
			frameTime = MIN(entry.Duration, t);
			break;
		case AnimationType::Repeat:
			frameTime = floatMod(t, entry.Duration);
			break;
		case AnimationType::PingPong:
			frameTime = floatMod(t, 2.0f * entry.Duration);
			if (frameTime > entry.Duration) frameTime = 2.0f * entry.Duration - frameTime;
			break;
		default:
			NOT_IMPLEMENTED;
		}

		for (uint32_t i = 1; i < entry.KeyFrames.size(); i++)
		{
			if (entry.KeyFrames[i].Time >= frameTime)
			{
				kfA = entry.KeyFrames[i - 1];
				kfB = entry.KeyFrames[i];
				break;
			}
		}
		animationTime = (frameTime - kfA.Time) / (kfB.Time - kfA.Time);
	}

	static DirectX::XMMATRIX ProcessAnimationEntry(const ModelLoading::AnimationEntry& entry, float t, AnimationType animationType)
	{
		ModelLoading::AnimKeyFrame keyFrameA;
		ModelLoading::AnimKeyFrame keyFrameB;
		float animationTime;
		CalculateTargetKeyframes(entry, t, keyFrameA, keyFrameB, animationTime, animationType);
		return GetFrameTransform(keyFrameA, keyFrameB, animationTime, entry.Interpolation, entry.Target);
	}

	static void AnimateWeight(float& weight, const ModelLoading::AnimationEntry& entry, float t, AnimationType animationType)
	{
		ModelLoading::AnimKeyFrame keyFrameA;
		ModelLoading::AnimKeyFrame keyFrameB;
		float animationTime;
		CalculateTargetKeyframes(entry, t, keyFrameA, keyFrameB, animationTime, animationType);
		weight += Interpolate(keyFrameA.Weight, keyFrameB.Weight, animationTime, entry.Interpolation);
	}

	DirectX::XMFLOAT4X4 GetAnimationTransformation(const std::vector<ModelLoading::AnimationEntry>& animations, float t, AnimationType animationType)
	{
		using namespace DirectX;

		XMMATRIX animationTransform = XMMatrixIdentity();
		for (const ModelLoading::AnimationEntry& entry : animations)
		{
			if (entry.Target == ModelLoading::AnimTarget::Weights) continue;

			const XMMATRIX entryTransform = ProcessAnimationEntry(entry, t, animationType);
			animationTransform = XMMatrixMultiply(animationTransform, entryTransform);
		}
		return XMUtility::ToXMFloat4x4(animationTransform);
	}

	std::vector<float> GetAnimatedMorphWeights(const ModelLoading::SceneObject& object, float t, AnimationType animationType)
	{
		using namespace DirectX;

		std::vector<float> weights{};
		for (const ModelLoading::MorphTarget& target : object.MorphTargets)
			weights.push_back(target.Weight);

		for (const ModelLoading::AnimationEntry& entry : object.AnimcationData)
		{
			if (entry.Target == ModelLoading::AnimTarget::Weights)
				AnimateWeight(weights[entry.WeightTargetIndex], entry, t, animationType);
		}
		return weights;
	}
}