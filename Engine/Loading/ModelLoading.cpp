#include "ModelLoading.h"

#pragma warning(disable : 4996)
#ifndef CGLTF_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#endif

#include "Common.h"
#include "Render/Device.h"
#include "Render/Buffer.h"
#include "Render/Texture.h"
#include "Render/Context.h"
#include "Render/Commands.h"
#include "Render/Memory.h"
#include "Render/RenderThread.h"
#include "Loading/TextureLoading.h"
#include "Utility/PathUtility.h"
#include "System/ApplicationConfiguration.h"

#define CGTF_CALL(X) { cgltf_result result = X; ASSERT(result == cgltf_result_success, "CGTF_CALL_FAIL") }

#undef min
#undef max
#undef OPAQUE

namespace ModelLoading
{
	static Float3 ToFloat3(cgltf_float color[3])
	{
		return Float3{ color[0], color[1], color[2] };
	}

	static Float4 ToFloat4(cgltf_float color[4])
	{
		return Float4{ color[0], color[1], color[2], color[3] };
	}

	template<typename T>
	static T* GetBufferData(cgltf_accessor* accessor)
	{
		unsigned char* buffer = (unsigned char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset;
		void* data = buffer + accessor->offset;
		return static_cast<T*>(data);
	}

	template<typename T>
	static void FetchIndexData(cgltf_accessor* indexAccessor, std::vector<uint32_t>& buffer)
	{
		T* indexData = GetBufferData<T>(indexAccessor);
		for (size_t i = 0; i < indexAccessor->count; i++)
		{
			buffer[i] = indexData[i];
		}
	}

	static void LoadIB(cgltf_accessor* indexAccessor, std::vector<uint32_t>& buffer)
	{
		ASSERT(indexAccessor, "[SceneLoading] Trying to read indices from empty accessor");
		ASSERT(indexAccessor->type == cgltf_type_scalar, "[SceneLoading] Indices of a mesh arent scalar.");

		buffer.resize(indexAccessor->count);

		switch (indexAccessor->component_type)
		{
		case cgltf_component_type_r_16u: FetchIndexData<uint16_t>(indexAccessor, buffer); break;
		case cgltf_component_type_r_32u: FetchIndexData<uint32_t>(indexAccessor, buffer); break;
		default: NOT_IMPLEMENTED;
		}
	}

	static void ValidateAnimationSampler(cgltf_animation_sampler* sampler, AnimTarget targetType)
	{
		ASSERT(sampler->output->count % sampler->input->count == 0, "sampler->output->count % sampler->input->count != 0");

		// Time data
		ASSERT(sampler->input->type == cgltf_type_scalar, "sampler->input->type != cgltf_type_scalar");
		ASSERT(sampler->input->component_type == cgltf_component_type_r_32f, "sampler->input->component_type == cgltf_component_type_r_32f");

		// Value data
		ASSERT(sampler->output->component_type == cgltf_component_type_r_32f, "sampler->output->component_type == cgltf_component_type_r_32f");
		switch (targetType)
		{
		case AnimTarget::Translation:
		case AnimTarget::Scale:
			ASSERT(sampler->output->type == cgltf_type_vec3, "sampler->output->type == cgltf_type_vec3");
			break;
		case AnimTarget::Rotation:
			ASSERT(sampler->output->type == cgltf_type_vec4, "sampler->output->type == cgltf_type_vec4");
			break;
		case AnimTarget::Weights:
			ASSERT(sampler->output->type == cgltf_type_scalar, "sampler->output->type == cgltf_type_scalar");
			break;
		case AnimTarget::Invalid:
		default:
			ASSERT(0, "Invalid sampling target type");
		}
	}

	template<cgltf_type TYPE, cgltf_component_type COMPONENT_TYPE>
	static void ValidateVertexAttribute(cgltf_attribute* attribute)
	{
		ASSERT(attribute->data->type == TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->type == TYPE");
		ASSERT(attribute->data->component_type == COMPONENT_TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->component_type == COMPONENT_TYPE");
	}

	struct VertexAttributesData
	{
		uint32_t NumVertices = 0;
		Float3* Positions = nullptr;
		Float2* Texcoords = nullptr;
		Float3* Normals = nullptr;
		Float4* Tangents = nullptr;
		Float4* Colors = nullptr;
		Float4* Weights = nullptr;

		// This is actually uint4
		uint16_t* Joints16 = nullptr; 
		uint8_t* Joints8 = nullptr;
	};

	void GetJointsData(VertexAttributesData& vertexData, cgltf_attribute* jointsData)
	{
		switch (jointsData->data->component_type)
		{
		case cgltf_component_type_r_8u:
			ValidateVertexAttribute<cgltf_type_vec4, cgltf_component_type_r_8u>(jointsData);
			vertexData.Joints8 = GetBufferData<uint8_t>(jointsData->data);
			break;
		case cgltf_component_type_r_16u:
			ValidateVertexAttribute<cgltf_type_vec4, cgltf_component_type_r_16u>(jointsData);
			vertexData.Joints16 = GetBufferData<uint16_t>(jointsData->data);
			break;
		default: NOT_IMPLEMENTED;
		}
	}

	VertexAttributesData LoadAttributes(cgltf_attribute* attributes, cgltf_size attributesNumber)
	{
		if (attributesNumber == 0) return VertexAttributesData{};

		VertexAttributesData attributesData;
		attributesData.NumVertices = (uint32_t) attributes->data->count;
		for (size_t i = 0; i < attributesNumber; i++)
		{
			cgltf_attribute* vertexAttribute = (attributes + i);
			switch (vertexAttribute->type)
			{
			case cgltf_attribute_type_position:
				ValidateVertexAttribute<cgltf_type_vec3, cgltf_component_type_r_32f>(vertexAttribute);
				attributesData.Positions = GetBufferData<Float3>(vertexAttribute->data);
				break;
			case cgltf_attribute_type_texcoord:
				ValidateVertexAttribute<cgltf_type_vec2, cgltf_component_type_r_32f>(vertexAttribute);
				attributesData.Texcoords = GetBufferData<Float2>(vertexAttribute->data);
				break;
			case cgltf_attribute_type_normal:
				ValidateVertexAttribute<cgltf_type_vec3, cgltf_component_type_r_32f>(vertexAttribute);
				attributesData.Normals = GetBufferData<Float3>(vertexAttribute->data);
				break;
			case cgltf_attribute_type_tangent:
				if (vertexAttribute->type != cgltf_type_vec4) continue; // Tmp hack: Find a way to fix this
				ValidateVertexAttribute<cgltf_type_vec4, cgltf_component_type_r_32f>(vertexAttribute);
				attributesData.Tangents = GetBufferData<Float4>(vertexAttribute->data);
				break;
			case cgltf_attribute_type_color:
				ValidateVertexAttribute<cgltf_type_vec4, cgltf_component_type_r_32f>(vertexAttribute);
				attributesData.Colors = GetBufferData<Float4>(vertexAttribute->data);
				break;
			case cgltf_attribute_type_joints:
				GetJointsData(attributesData, vertexAttribute);
				break;
			case cgltf_attribute_type_weights:
				ValidateVertexAttribute<cgltf_type_vec4, cgltf_component_type_r_32f>(vertexAttribute);
				attributesData.Weights = GetBufferData<Float4>(vertexAttribute->data);
				break;
			default:
				NOT_IMPLEMENTED;
				break;
			}
		}
		return attributesData;
	}

	static AnimTarget ToAnimTarget(cgltf_animation_path_type pathType)
	{
		switch (pathType)
		{
			case cgltf_animation_path_type_invalid:			return AnimTarget::Invalid;
			case cgltf_animation_path_type_translation:		return AnimTarget::Translation;
			case cgltf_animation_path_type_rotation:		return AnimTarget::Rotation;
			case cgltf_animation_path_type_scale:			return AnimTarget::Scale;
			case cgltf_animation_path_type_weights:			return AnimTarget::Weights;
			default: NOT_IMPLEMENTED;
		}
		return AnimTarget::Invalid;
	}

	static AnimInterpolation ToInterpolationType(cgltf_interpolation_type interpolationType, AnimTarget target)
	{
		switch (interpolationType)
		{
		case cgltf_interpolation_type_cubic_spline:		// return AnimInterpolation::Cubic; Not implemented
		case cgltf_interpolation_type_linear:			return target == AnimTarget::Rotation ? AnimInterpolation::SLerp : AnimInterpolation::Lerp;
		case cgltf_interpolation_type_step:				return AnimInterpolation::Step;
		default: NOT_IMPLEMENTED;
		}
		return AnimInterpolation::Invalid;
	}

	static BoundingSphere CalculateBoundingSphere(cgltf_primitive* meshData)
	{
		VertexAttributesData vertexData = LoadAttributes(meshData->attributes, meshData->attributes_count);
		if (vertexData.Positions == nullptr || vertexData.NumVertices == 0) return BoundingSphere{};

		static constexpr float MAX_FLOAT = std::numeric_limits<float>::max();
		static constexpr float MIN_FLOAT = -MAX_FLOAT;

		Float3 minAABB{ MAX_FLOAT , MAX_FLOAT, MAX_FLOAT };
		Float3 maxAABB{ MIN_FLOAT, MIN_FLOAT, MIN_FLOAT };
		for (uint32_t i = 0; i < vertexData.NumVertices; i++)
		{
			const Float3 pos = vertexData.Positions[i];

			minAABB.x = MIN(minAABB.x, pos.x);
			minAABB.y = MIN(minAABB.y, pos.y);
			minAABB.z = MIN(minAABB.z, pos.z);

			maxAABB.x = MAX(maxAABB.x, pos.x);
			maxAABB.y = MAX(maxAABB.y, pos.y);
			maxAABB.z = MAX(maxAABB.z, pos.z);
		}

		BoundingSphere bs;
		bs.Center = 0.5f * (maxAABB + minAABB);
		bs.Radius = (minAABB - maxAABB).LengthFast() * 0.5f;

		return bs;
	}

	static DirectX::XMFLOAT4X4 CalcBaseTransform(cgltf_node* nodeData, const Float3& positionOrigin, const Float3& baseScale)
	{
		using namespace DirectX;

		std::vector<cgltf_node*> hierarchy;
		cgltf_node* currNode = nodeData;
		while (currNode)
		{
			hierarchy.push_back(currNode);
			currNode = currNode->parent;
		}

		XMMATRIX transform = XMMatrixIdentity();
		for (int32_t i = (int32_t) hierarchy.size() - 1; i >= 0; i--)
		{
			cgltf_node* node = hierarchy[i];
			ASSERT(!node->has_matrix, "Currently matrix transform representations not supported!");
			const Float3 position = node->has_translation ? ToFloat3(node->translation) : Float3{ 0.0f, 0.0f, 0.0f };
			const Float4 rotation = node->has_rotation ? ToFloat4(node->rotation) : Float4{ 0.0f, 0.0f, 0.0f, 0.0f };
			const Float3 scale = node->has_scale ? ToFloat3(node->scale) : Float3{ 1.0f, 1.0f, 1.0f };
			const XMMATRIX nodeTransform = XMMatrixAffineTransformation(scale, Float3{ 0.0f, 0.0f, 0.0f }, rotation, position);
			transform = XMMatrixMultiply(transform, nodeTransform);
		}
		transform = XMMatrixMultiply(transform, XMMatrixAffineTransformation(baseScale, Float3{ 0.0f, 0.0f, 0.0f }, Float4(0.0f, 0.0f, 0.0f, 0.0f), positionOrigin));
		return XMUtility::ToXMFloat4x4(transform);
	}

	std::vector<SceneObject> Loader::Load(const std::string& path)
	{
		const std::string& ext = PathUtility::GetFileExtension(path);
		if (ext != "gltf")
		{
			ASSERT(0, "[SceneLoading] For now we only support glTF 3D format.");
			return {};
		}
		m_DirectoryPath = PathUtility::GetPathWitoutFile(path);

		cgltf_options options = {};
		cgltf_data* data = NULL;
		CGTF_CALL(cgltf_parse_file(&options, path.c_str(), &data));
		CGTF_CALL(cgltf_load_buffers(&options, data, path.c_str()));

		m_Scene = std::vector<SceneObject>();

		FillNodeAnimationMap(data);
		for (size_t i = 0; i < data->nodes_count; i++) LoadNode(data->nodes + i);

		cgltf_free(data);

		return m_Scene;
	}

	void Loader::FillNodeAnimationMap(cgltf_data* sceneData)
	{
		m_NodeAnimationMap.clear();

		for (cgltf_size animIndex = 0; animIndex < sceneData->animations_count; animIndex++)
		{
			cgltf_animation* animation = sceneData->animations + animIndex;

			for (cgltf_size channelIndex = 0; channelIndex < animation->channels_count; channelIndex++)
			{
				cgltf_animation_channel* channel = animation->channels + channelIndex;
				cgltf_node* node = channel->target_node;
				if (!m_NodeAnimationMap.contains(node))
					m_NodeAnimationMap[node] = std::vector<cgltf_animation_channel*>{};
				m_NodeAnimationMap[node].push_back(channel);
			}
		}
	}

	void Loader::LoadNode(cgltf_node* nodeData)
	{
		std::vector<AnimationEntry> animationData = LoadAnimations(nodeData);
		if (nodeData->mesh)
		{
			const DirectX::XMFLOAT4X4 transform = CalcBaseTransform(nodeData, m_PositionOrigin, m_BaseScale);
			cgltf_mesh* mesh = nodeData->mesh;

			std::vector<float> morphWeights{};
			for (cgltf_size i = 0; i < mesh->weights_count; i++) morphWeights.push_back(*(mesh->weights + i));

			for (cgltf_size i = 0; i < mesh->primitives_count; i++)
			{
				cgltf_primitive* primitive = mesh->primitives + i;
				LoadObject(primitive);
				LoadMorph(primitive, morphWeights);
				LoadSkin(nodeData->skin);

				m_Object.ModelToWorld = transform;
				m_Object.AnimcationData = animationData;
				m_Scene.push_back(m_Object);
			}
		}
	}

	std::vector<ModelLoading::AnimationEntry> Loader::LoadAnimations(cgltf_node* nodeData)
	{
		if (!m_NodeAnimationMap.contains(nodeData)) return {};

		std::vector<ModelLoading::AnimationEntry> animations{};
		for (const cgltf_animation_channel* animationChannel : m_NodeAnimationMap[nodeData])
		{
			cgltf_animation_sampler* animationSampler = animationChannel->sampler;

			// Validate
			const AnimTarget animationTarget = ToAnimTarget(animationChannel->target_path);
			const AnimInterpolation animationInterpolation = ToInterpolationType(animationSampler->interpolation, animationTarget);
			ValidateAnimationSampler(animationSampler, animationTarget);

			// Load data
			float* timeData = GetBufferData<float>(animationSampler->input);
			uint8_t* valueData = GetBufferData<uint8_t>(animationSampler->output);
			Float4* valueDataF4 = (Float4*)valueData;
			Float3* valueDataF3 = (Float3*)valueData;
			float* valueDataF = (float*)valueData;

			// Insert entries
			const cgltf_size entriesCount = animationSampler->output->count / animationSampler->input->count;
			ASSERT(entriesCount == 1 || animationTarget == AnimTarget::Weights, "entriesCount == 1 || animationTarget == AnimTarget::Weights");
			for (cgltf_size i = 0; i < entriesCount; i++)
			{
				const cgltf_size keyFrameCount = animationSampler->input->count;

				// Create entry
				AnimationEntry entry{};
				entry.Target = animationTarget;
				entry.Interpolation = animationInterpolation;
				entry.Duration = *(timeData + keyFrameCount - 1);
				entry.KeyFrames.resize(keyFrameCount);
				entry.WeightTargetIndex = (uint32_t) i;

				// Insert keyframes
				for (cgltf_size keyFrameIndex = 0; keyFrameIndex < keyFrameCount; keyFrameIndex++)
				{
					entry.KeyFrames[keyFrameIndex].Time = *(timeData + keyFrameIndex);

					const cgltf_size valueIndex = i + keyFrameIndex * entriesCount;
					switch (entry.Target)
					{
					case AnimTarget::Translation:
						entry.KeyFrames[keyFrameIndex].Translation = *(valueDataF3 + valueIndex);
						break;
					case AnimTarget::Rotation:
						entry.KeyFrames[keyFrameIndex].Rotation = *(valueDataF4 + valueIndex);
						break;
					case AnimTarget::Scale:
						entry.KeyFrames[keyFrameIndex].Scale = *(valueDataF3 + valueIndex);
						break;
					case AnimTarget::Weights:
						entry.KeyFrames[keyFrameIndex].Weight = *(valueDataF + valueIndex);
						break;
					case AnimTarget::Invalid:
					default:
						NOT_IMPLEMENTED;

					}
				}
				animations.push_back(entry);
			}
		}

		return animations;
	}

	void Loader::LoadMorph(cgltf_primitive* meshData, const std::vector<float>& weights)
	{
		if (weights.empty()) return;

		ASSERT(weights.size() == meshData->targets_count, "weights.size() != meshData->targets_count");

		for (cgltf_size targetIndex = 0; targetIndex < meshData->targets_count; targetIndex++)
		{
			cgltf_morph_target* morphTarget = meshData->targets + targetIndex;

			VertexAttributesData vertices = LoadAttributes(morphTarget->attributes, morphTarget->attributes_count);
			ASSERT(!vertices.Joints8 && !vertices.Joints16 && !vertices.Weights && !vertices.Colors, "[Loader::LoadMorph] Using not supported morph vertex attributes");

			std::vector<MorphVertex> morphTargetData{};
			morphTargetData.resize(vertices.NumVertices);
			for (uint32_t i = 0; i < vertices.NumVertices; i++)
			{
				if (vertices.Positions) morphTargetData[i].Position = *(vertices.Positions + i);
				if (vertices.Texcoords) morphTargetData[i].Texcoord = *(vertices.Texcoords + i);
				if (vertices.Normals) morphTargetData[i].Normal = *(vertices.Normals + i);
				if (vertices.Tangents) morphTargetData[i].Tangent = *(vertices.Tangents + i);
			}
			
			ResourceInitData initData{};
			initData.Context = &m_Context;
			initData.Data = morphTargetData.data();

			MorphTarget targetData{};
			targetData.Weight = weights[targetIndex];
			targetData.Data = GFX::CreateBuffer(vertices.NumVertices * sizeof(MorphVertex), sizeof(MorphVertex), RCF_None, &initData);

			m_Object.MorphTargets.push_back(targetData);
		}
	}

	void Loader::LoadSkin(cgltf_skin* skinData)
	{
		if (!skinData) return;

		DirectX::XMFLOAT4X4* jointMatrices = GetBufferData<DirectX::XMFLOAT4X4>(skinData->inverse_bind_matrices);
		for (cgltf_size i = 0; i < skinData->joints_count; i++)
		{
			cgltf_node* jointNode = *(skinData->joints + i);

			SkeletonJoint joint;
			joint.ModelToJoint = *(jointMatrices + i);
			joint.Transform = CalcBaseTransform(jointNode, m_PositionOrigin, m_BaseScale);
			joint.Animations = LoadAnimations(jointNode);
			m_Object.Skeleton.push_back(joint);
		}
	}

	void Loader::LoadObject(cgltf_primitive* objectData)
	{
		m_Object = SceneObject{};
		m_Object.BoundingVolume = CalculateBoundingSphere(objectData);
		LoadMesh(objectData);
		LoadMaterial(objectData->material);
	}

	void Loader::LoadMesh(cgltf_primitive* meshData)
	{
		ASSERT(meshData->type == cgltf_primitive_type_triangles, "[SceneLoading] Scene contains quad meshes. We are supporting just triangle meshes.");

		const uint32_t vertCount = (uint32_t) meshData->attributes[0].data->count;

		const VertexAttributesData vertices = LoadAttributes(meshData->attributes, meshData->attributes_count);
		std::vector<uint32_t> joints;

		if (vertices.Joints8)
		{
			joints.resize(vertices.NumVertices * 4);
			for (uint16_t i = 0; i < vertices.NumVertices * 4; i++) 
				joints[i] = vertices.Joints8[i];
		}

		if (vertices.Joints16)
		{
			joints.resize(vertices.NumVertices * 4);
			for (uint16_t i = 0; i < vertices.NumVertices * 4; i++)
				joints[i] = vertices.Joints16[i];
		}

		std::vector<uint32_t> indices;
		if(meshData->indices)
			LoadIB(meshData->indices, indices);

		const auto createBuffer = [this](const void* data, uint32_t stride, uint32_t numElements)
		{
			std::vector<uint8_t> defaultData{};
			if (!data)
			{
				defaultData.resize(stride * numElements);
				memset(defaultData.data(), 0, stride * numElements);
				data = defaultData.data();
			}
			ResourceInitData initData{ &this->m_Context, data };
			return GFX::CreateBuffer(numElements * stride, stride, RCF_None, &initData);
		};

		m_Object.Positions = createBuffer(vertices.Positions, sizeof(DirectX::XMFLOAT3), vertCount);
		m_Object.Texcoords = createBuffer(vertices.Texcoords, sizeof(DirectX::XMFLOAT2), vertCount);
		m_Object.Normals = createBuffer(vertices.Normals, sizeof(DirectX::XMFLOAT3), vertCount);
		m_Object.Tangents = createBuffer(vertices.Tangents, sizeof(DirectX::XMFLOAT4), vertCount);
		m_Object.Weights = createBuffer(vertices.Weights, sizeof(Float4), vertCount);
		m_Object.Joints = createBuffer(joints.empty() ? nullptr : joints.data(), sizeof(uint32_t) * 4, vertCount);
		m_Object.Indices = createBuffer(indices.empty() ? nullptr : indices.data(), sizeof(uint32_t), (uint32_t) indices.size());
	}

	void Loader::LoadMaterial(cgltf_material* materialData)
	{
		if (!materialData || !materialData->has_pbr_metallic_roughness)
		{
			m_Object.Albedo = LoadTexture(nullptr);
			m_Object.Normal = LoadTexture(nullptr, ColorUNORM(0.5f, 0.5f, 1.0f, 1.0f));
			m_Object.MetallicRoughness = LoadTexture(nullptr);
			return;
		}

		cgltf_pbr_metallic_roughness& mat = materialData->pbr_metallic_roughness;
		
		switch (materialData->alpha_mode)
		{
		case cgltf_alpha_mode_blend:
			m_Object.MatType = SceneObject::MaterialType::ALPHA_BLEND; break;
		case cgltf_alpha_mode_mask:
			m_Object.MatType = SceneObject::MaterialType::ALPHA_DISCARD; break;
		case cgltf_alpha_mode_opaque:
			m_Object.MatType = SceneObject::MaterialType::OPAQUE; break;
		default:
			NOT_IMPLEMENTED;
		}

		m_Object.AlbedoFactor = ToFloat3(mat.base_color_factor);
		m_Object.MetallicFactor = mat.metallic_factor;
		m_Object.RoughnessFactor = mat.roughness_factor;

		m_Object.Albedo = LoadTexture(mat.base_color_texture.texture);
		m_Object.Normal = LoadTexture(materialData->normal_texture.texture, ColorUNORM(0.5f, 0.5f, 1.0f, 1.0f));
		m_Object.MetallicRoughness = LoadTexture(mat.metallic_roughness_texture.texture);
	}

	Texture* Loader::LoadTexture(cgltf_texture* textureData, ColorUNORM defaultColor)
	{
		if (textureData)
		{
			const std::string textureURI = textureData->image->uri;
			const std::string texturePath = m_DirectoryPath + "/" + textureURI;
			return TextureLoading::LoadTexture(m_Context, texturePath, RCF_None, m_TextureNumMips);
		}
		else
		{
			ResourceInitData initData = { &m_Context, &defaultColor };
			return GFX::CreateTexture(1, 1, RCF_None, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &initData);
		}
	}

	template<typename T>
	static void Free(T** res)
	{
		if (*res) DeferredTrash::Put(*res);
		*res = nullptr;
	}

	void Free(SceneObject& sceneObject)
	{
		Free(&sceneObject.Positions);
		Free(&sceneObject.Texcoords);
		Free(&sceneObject.Normals);
		Free(&sceneObject.Tangents);
		Free(&sceneObject.Weights);
		Free(&sceneObject.Joints);
		Free(&sceneObject.Indices);
		Free(&sceneObject.Albedo);
		Free(&sceneObject.Normal);
		Free(&sceneObject.MetallicRoughness);

		for (MorphTarget& morphTarget : sceneObject.MorphTargets)
		{
			DeferredTrash::Put(morphTarget.Data);
		}
	}
	
	void Free(std::vector<SceneObject>& scene)
	{
		for (SceneObject& sceneObject : scene) Free(sceneObject);
	}
}


