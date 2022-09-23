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

	template<cgltf_type TYPE, cgltf_component_type COMPONENT_TYPE>
	static void ValidateVertexAttribute(cgltf_attribute* attribute)
	{
		ASSERT(attribute->data->type == TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->type == TYPE");
		ASSERT(attribute->data->component_type == COMPONENT_TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->component_type == COMPONENT_TYPE");
	}

	static BoundingSphere CalculateBoundingSphere(cgltf_primitive* meshData)
	{
		Float3* vertexData = nullptr;
		uint32_t vertexCount = (uint32_t) meshData->attributes[0].data->count;

		for (size_t i = 0; i < meshData->attributes_count && vertexData == nullptr; i++)
		{
			cgltf_attribute* vertexAttribute = (meshData->attributes + i);
			switch (vertexAttribute->type)
			{
			case cgltf_attribute_type_position:
				ValidateVertexAttribute<cgltf_type_vec3, cgltf_component_type_r_32f>(vertexAttribute);
				vertexData = GetBufferData<Float3>(vertexAttribute->data);
				break;
			}
		}

		ASSERT(vertexData, "[SceneLoading] ASSERT FAILED: vertexData");

		static constexpr float MAX_FLOAT = std::numeric_limits<float>::max();
		static constexpr float MIN_FLOAT = -MAX_FLOAT;

		Float3 minAABB{ MAX_FLOAT , MAX_FLOAT, MAX_FLOAT };
		Float3 maxAABB{ MIN_FLOAT, MIN_FLOAT, MIN_FLOAT };
		for (uint32_t i = 0; i < vertexCount; i++)
		{
			const Float3 pos = vertexData[i];

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

	static DirectX::XMFLOAT4X4 CalcBaseTransform(cgltf_node* nodeData)
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
			if (node->has_matrix)
			{
				NOT_IMPLEMENTED;
			}
			else
			{
				const Float3 position = node->has_translation ? ToFloat3(node->translation) : Float3{ 0.0f, 0.0f, 0.0f };
				const Float4 rotation = node->has_rotation ? ToFloat4(node->rotation) : Float4{ 0.0f, 0.0f, 0.0f, 0.0f };
				const Float3 scale = node->has_scale ? ToFloat3(node->scale) : Float3{ 1.0f, 1.0f, 1.0f };
				const XMMATRIX nodeTransform = XMMatrixAffineTransformation(scale, Float3{ 0.0f, 0.0f, 0.0f }, rotation, position);
				transform = XMMatrixMultiply(transform, nodeTransform);
			}
		}
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
		for (size_t i = 0; i < data->nodes_count; i++) LoadNode(data->nodes + i);

		cgltf_free(data);

		return m_Scene;
	}

	void Loader::LoadNode(cgltf_node* nodeData)
	{
		if (nodeData->mesh)
		{
			const DirectX::XMFLOAT4X4 transform = CalcBaseTransform(nodeData);

			for (size_t i = 0; i < nodeData->mesh->primitives_count; i++)
			{
				LoadObject(nodeData->mesh->primitives + i);
				m_Object.ModelToWorld = transform;
				m_Scene.push_back(m_Object);
			}
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

		Float3* positionData = nullptr;
		Float2* uvData = nullptr;
		Float3* normalData = nullptr;
		Float4* tangentData = nullptr;

		for (size_t i = 0; i < meshData->attributes_count; i++)
		{
			cgltf_attribute* vertexAttribute = (meshData->attributes + i);
			switch (vertexAttribute->type)
			{
			case cgltf_attribute_type_position:
				ValidateVertexAttribute<cgltf_type_vec3, cgltf_component_type_r_32f>(vertexAttribute);
				positionData = GetBufferData<Float3>(vertexAttribute->data);
				break;
			case cgltf_attribute_type_texcoord:
				ValidateVertexAttribute<cgltf_type_vec2, cgltf_component_type_r_32f>(vertexAttribute);
				uvData = GetBufferData<Float2>(vertexAttribute->data);
				break;
			case cgltf_attribute_type_normal:
				ValidateVertexAttribute<cgltf_type_vec3, cgltf_component_type_r_32f>(vertexAttribute);
				normalData = GetBufferData<Float3>(vertexAttribute->data);
				break;
			case cgltf_attribute_type_tangent:
				ValidateVertexAttribute<cgltf_type_vec4, cgltf_component_type_r_32f>(vertexAttribute);
				tangentData = GetBufferData<Float4>(vertexAttribute->data);
				break;
			}
		}

		// TODO: calculate missing attributes
		ASSERT(positionData && uvData && normalData, "[Loader::LoadMesh] Missing vertex data");

		// Indices
		std::vector<uint32_t> indices;
		LoadIB(meshData->indices, indices);

		// Vertices
		std::vector<ModelLoading::Vertex> vertices;
		vertices.resize(vertCount);
		for (uint32_t i = 0; i < vertCount; i++)
		{
			vertices[i].Position = positionData[i];
			vertices[i].Texcoord = uvData[i];
			vertices[i].Normal = normalData[i];
			if (tangentData) vertices[i].Tangent = tangentData[i];
		}

		ResourceInitData verticesData{&m_Context, vertices.data()};
		ResourceInitData indicesData{ &m_Context, indices.data() };

		m_Object.Vertices = GFX::CreateBuffer((UINT) vertices.size() * sizeof(ModelLoading::Vertex), sizeof(ModelLoading::Vertex), RCF_None, &verticesData);
		m_Object.Indices = GFX::CreateBuffer((UINT) indices.size() * sizeof(uint32_t), sizeof(uint32_t), RCF_None, &indicesData);
	}

	void Loader::LoadMaterial(cgltf_material* materialData)
	{
		if (!materialData || !materialData->has_pbr_metallic_roughness) return;

		cgltf_pbr_metallic_roughness& mat = materialData->pbr_metallic_roughness;
		
		switch (materialData->alpha_mode)
		{
		case cgltf_alpha_mode_blend:
			m_Object.BlendMode = SceneObject::ALPHA_BLEND; break;
		case cgltf_alpha_mode_mask:
			m_Object.BlendMode = SceneObject::ALPHA_DISCARD; break;
		case cgltf_alpha_mode_opaque:
			m_Object.BlendMode = SceneObject::OPAQUE; break;
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
		if (!textureData && !m_LoadDefaultTextures) return nullptr;
		
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
		Free(&sceneObject.Vertices);
		Free(&sceneObject.Indices);
		Free(&sceneObject.Albedo);
		Free(&sceneObject.Normal);
		Free(&sceneObject.MetallicRoughness);
	}
	
	void Free(std::vector<SceneObject>& scene)
	{
		for (SceneObject& sceneObject : scene) Free(sceneObject);
	}
}


