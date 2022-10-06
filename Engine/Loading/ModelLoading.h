#pragma once

#include <unordered_map>
#include <vector>

#include "Common.h"

struct Buffer;
struct Texture;
struct GraphicsContext;

struct cgltf_data;
struct cgltf_node;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_texture;
struct cgltf_animation_channel;
struct cgltf_skin;

#undef OPAQUE

namespace ModelLoading
{
	enum class AnimTarget
	{
		Invalid,
		Translation,
		Rotation,
		Scale,
		Weights
	};

	enum class AnimInterpolation
	{
		Invalid,
		Step,
		Lerp,
		SLerp,
		Cubic,
	};

	struct AnimKeyFrame
	{
		float Time;
		union
		{
			Float4 Rotation = Float4{0.0f, 0.0f, 0.0f, 0.0f};
			Float3 Translation;
			Float3 Scale;
			float Weight;
		};

	};

	struct AnimationEntry
	{
		uint32_t WeightTargetIndex = 0;
		AnimTarget Target = AnimTarget::Invalid;
		AnimInterpolation Interpolation = AnimInterpolation::Invalid;
		std::vector<AnimKeyFrame> KeyFrames = {};
		float Duration = 1.0f;
	};

	struct MorphVertex
	{
		Float3 Position;
		Float2 Texcoord;
		Float3 Normal;
		Float4 Tangent;
	};

	struct MorphTarget
	{
		float Weight;
		Buffer* Data; // MorphVertex
	};

	struct SkeletonJoint
	{
		DirectX::XMFLOAT4X4 Transform;
		DirectX::XMFLOAT4X4 ModelToJoint;
		std::vector<AnimationEntry> Animations;
	};

	struct BoundingSphere
	{
		Float3 Center{ 0.0f, 0.0f, 0.0f };
		float Radius = 1.0f;
	};

	struct SceneObject
	{
		enum class MaterialType 
		{
			OPAQUE, 
			ALPHA_DISCARD, 
			ALPHA_BLEND
		};

		BoundingSphere BoundingVolume;
		DirectX::XMFLOAT4X4 ModelToWorld;

		Buffer* Positions = nullptr;	// float3
		Buffer* Texcoords = nullptr;	// float2
		Buffer* Normals = nullptr;		// float3 
		Buffer* Tangents = nullptr;		// float4
		Buffer* Weights = nullptr;		// float4
		Buffer* Joints = nullptr;		// uint4
		Buffer* Indices = nullptr;		// uint32_t

		MaterialType MatType = MaterialType::OPAQUE;

		Float3 AlbedoFactor{ 1.0f, 1.0f, 1.0f };
		float MetallicFactor = 1.0f;
		float RoughnessFactor = 1.0f;

		Texture* Albedo = nullptr;
		Texture* Normal = nullptr;
		Texture* MetallicRoughness = nullptr;

		std::vector<AnimationEntry> AnimcationData;
		std::vector<MorphTarget> MorphTargets;
		std::vector<SkeletonJoint> Skeleton;
	};

	class Loader
	{
	public:
		Loader(GraphicsContext& context): 
			m_Context(context)
		{}

		std::vector<SceneObject> Load(const std::string& path);

		void SetPositionOrigin(const Float3& origin) { m_PositionOrigin = origin; }
		void SetBaseScale(const Float3& scale) { m_BaseScale = scale; }

	private:
		void FillNodeAnimationMap(cgltf_data* sceneData);
		void LoadNode(cgltf_node* nodeData);
		std::vector<AnimationEntry> LoadAnimations(cgltf_node* nodeData);
		void LoadMorph(cgltf_primitive* meshData, const std::vector<float>& weights);
		void LoadSkin(cgltf_skin* skinData);
;		void LoadObject(cgltf_primitive* objectData);
		void LoadMesh(cgltf_primitive* meshData);
		void LoadMaterial(cgltf_material* materialData);
		Texture* LoadTexture(cgltf_texture* textureData, ColorUNORM defaultColor = {0.0f, 0.0f, 0.0f, 0.0f});

	private:
		// Configurations
		GraphicsContext& m_Context;
		uint32_t m_TextureNumMips = 1;
		Float3 m_PositionOrigin{ 0.0f, 0.0f, 0.0f };
		Float3 m_BaseScale{ 1.0f, 1.0f, 1.0f };

		// Intermediate variables
		std::vector<SceneObject> m_Scene;
		SceneObject m_Object;
		std::string m_DirectoryPath;
		std::unordered_map<cgltf_node*, std::vector<cgltf_animation_channel*>> m_NodeAnimationMap;
	};

	void Free(SceneObject& sceneObject);
	void Free(std::vector<SceneObject>& scene);
}