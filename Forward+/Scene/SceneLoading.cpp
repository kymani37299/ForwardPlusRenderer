#include "SceneLoading.h"

#pragma warning (disable : 4996)
#ifndef CGLTF_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#endif

#include <Engine/Render/Device.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Texture.h>
#include <Engine/Render/Context.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/RenderThread.h>
#include <Engine/Utility/PathUtility.h>
#include <Engine/System/ApplicationConfiguration.h>

#include "Scene/TextureLoading.h"
#include "Shaders/shared_definitions.h"

#define CGTF_CALL(X) { cgltf_result result = X; ASSERT(result == cgltf_result_success, "CGTF_CALL_FAIL") }

// TMP_HACK
#undef min
#undef max

namespace SceneLoading
{
	namespace
	{
		struct LoadingContext
		{
			std::string RelativePath;
			GraphicsContext* GfxContext;
			SceneGraph* LoadingScene;
			RenderGroup* LoadingRG;
			RenderGroupType RGType;
		};

		static Float3 ToFloat3(cgltf_float color[3])
		{
			return Float3{ color[0], color[1], color[2] };
		}

		static Float4 ToFloat4(cgltf_float color[4])
		{
			return Float4{ color[0], color[1], color[2], color[3] };
		}

		template<typename T>
		T* GetBufferData(cgltf_accessor* accessor)
		{
			unsigned char* buffer = (unsigned char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset;
			void* data = buffer + accessor->offset;
			return static_cast<T*>(data);
		}

		void PrepareIB(const LoadingContext& context, Float3* positons, uint32_t numPositions, std::vector<uint32_t>& inBuffer, std::vector<uint32_t>& outBuffer, std::vector<DirectX::CullData>& outCullData)
		{
			std::vector<DirectX::Meshlet> meshlets;
			std::vector<uint8_t> uniqueVertexIB;
			std::vector<DirectX::MeshletTriangle> triangles;
			
			API_CALL(DirectX::ComputeMeshlets(inBuffer.data(), inBuffer.size() / 3, (DirectX::XMFLOAT3*) positons, numPositions, nullptr, meshlets, uniqueVertexIB, triangles, 128, MESHLET_TRIANGLE_COUNT));
			
			outCullData.resize(meshlets.size());
			uint32_t* vertexIBRaw = (uint32_t*) uniqueVertexIB.data();

			API_CALL(DirectX::ComputeCullData((DirectX::XMFLOAT3*)positons, numPositions, meshlets.data(), meshlets.size(), vertexIBRaw, uniqueVertexIB.size() / 4, triangles.data(), triangles.size(), outCullData.data()));

			outBuffer.reserve(meshlets.size() * MESHLET_INDEX_COUNT);

			for (const DirectX::Meshlet& meshlet : meshlets)
			{
				for (uint32_t i = 0; i < meshlet.PrimCount; i++)
				{
					DirectX::MeshletTriangle t = triangles[meshlet.PrimOffset + i];
					outBuffer.push_back(vertexIBRaw[meshlet.VertOffset + t.i0]);
					outBuffer.push_back(vertexIBRaw[meshlet.VertOffset + t.i1]);
					outBuffer.push_back(vertexIBRaw[meshlet.VertOffset + t.i2]);
				}
				for (uint32_t i = meshlet.PrimCount; i < MESHLET_TRIANGLE_COUNT; i++)
				{
					outBuffer.push_back(0);
					outBuffer.push_back(0);
					outBuffer.push_back(0);
				}
			}
		}

		template<typename T>
		void FetchIndexData(cgltf_accessor* indexAccessor, std::vector<uint32_t>& buffer)
		{
			T* indexData = GetBufferData<T>(indexAccessor);
			for (size_t i = 0; i < indexAccessor->count; i++)
			{
				buffer[i] = indexData[i];
			}
		}

		void LoadIB(const LoadingContext& context, cgltf_accessor* indexAccessor, std::vector<uint32_t>& buffer)
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
		void ValidateVertexAttribute(cgltf_attribute* attribute)
		{
			ASSERT(attribute->data->type == TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->type == TYPE");
			ASSERT(attribute->data->component_type == COMPONENT_TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->component_type == COMPONENT_TYPE");
		}

		uint32_t LoadMesh(const LoadingContext& context, cgltf_primitive* meshData)
		{
			ASSERT(meshData->type == cgltf_primitive_type_triangles, "[SceneLoading] Scene contains quad meshes. We are supporting just triangle meshes.");

			const uint32_t vertCount = meshData->attributes[0].data->count;

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
			ASSERT(positionData && uvData && normalData, "[SceneLoading::LoadMesh] Missing vertex data");
			
			// Indices
			std::vector<uint32_t> loadedIndices;
			LoadIB(context, meshData->indices, loadedIndices);

			std::vector<DirectX::CullData> cullData;
			std::vector<uint32_t> indices = loadedIndices; // TODO: Enable meshlets
			// PrepareIB(context, positionData, vertCount, loadedIndices, indices, cullData);

			// Vertices
			std::vector<MeshStorage::Vertex> vertices;
			vertices.resize(vertCount);
			for (uint32_t i = 0; i < vertCount; i++)
			{
				vertices[i].Position = positionData[i];
				vertices[i].Texcoord = uvData[i];
				vertices[i].Normal = normalData[i];
				vertices[i].Tangent = tangentData ? tangentData[i] : Float4(0,0,0,0);
			}

			MeshStorage& meshStorage = context.LoadingRG->MeshData;
			MeshStorage::Allocation alloc = meshStorage.Allocate(*context.GfxContext, vertCount, indices.size());

			Mesh mesh;
			mesh.VertCount = vertCount;
			mesh.IndexCount = indices.size();
			mesh.VertOffset = alloc.VertexOffset;
			mesh.IndexOffset = alloc.IndexOffset;
			mesh.MeshletCullData = cullData;

			// Upload
			GFX::Cmd::UploadToBuffer(*context.GfxContext, meshStorage.GetVertexBuffer(), mesh.VertOffset * MeshStorage::GetVertexBufferStride(), vertices.data(), 0, mesh.VertCount * MeshStorage::GetVertexBufferStride());
			GFX::Cmd::UploadToBuffer(*context.GfxContext, meshStorage.GetIndexBuffer(), mesh.IndexOffset * MeshStorage::GetIndexBufferStride(), indices.data(), 0, mesh.IndexCount * MeshStorage::GetIndexBufferStride());

			return context.LoadingRG->AddMesh(*context.GfxContext, mesh);
		}

		BoundingSphere CalculateBoundingSphere(cgltf_primitive* meshData)
		{
			Float3* vertexData = nullptr;
			uint32_t vertexCount = meshData->attributes[0].data->count;

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

		uint32_t LoadTexture(const LoadingContext& context, cgltf_texture* texture, ColorUNORM defaultColor = {1.0f, 1.0f, 1.0f, 1.0f})
		{
			TextureStorage::Allocation alloc = context.LoadingRG->TextureData.AllocTexture(*context.GfxContext);

			std::string texturePath = "";
			if (texture)
			{
				const std::string textureURI = texture->image->uri;
				texturePath = context.RelativePath + "/" + textureURI;
			}

			if (AppConfig.Settings.contains("MTR_LOADING"))
			{
				RenderThreadPool::Get()->Submit(new TextureLoadingTask(texturePath, alloc, context.LoadingRG->TextureData, defaultColor));
			}
			else
			{
				Device::Get()->GetTaskExecutor().Submit(new TextureLoadingTask(texturePath, alloc, context.LoadingRG->TextureData, defaultColor));
			}
			
			return alloc.TextureIndex;
		}

		RenderGroupType GetRenderGroupType(const Material& material)
		{
			RenderGroupType rgType = RenderGroupType::Opaque;
			if (material.UseBlend) rgType = RenderGroupType::Transparent;
			else if (material.UseAlphaDiscard) rgType = RenderGroupType::AlphaDiscard;
			return rgType;
		}

		uint32_t LoadMaterial(LoadingContext& context, cgltf_material* materialData)
		{
			ASSERT(materialData->has_pbr_metallic_roughness, "[SceneLoading] Every material must have a base color texture!");

			cgltf_pbr_metallic_roughness& mat = materialData->pbr_metallic_roughness;

			Material material;
			material.UseBlend = materialData->alpha_mode == cgltf_alpha_mode_blend;
			material.UseAlphaDiscard = materialData->alpha_mode == cgltf_alpha_mode_mask;
			material.AlbedoFactor = ToFloat3(mat.base_color_factor);
			material.MetallicFactor = mat.metallic_factor;
			material.RoughnessFactor = mat.roughness_factor;

			context.RGType = GetRenderGroupType(material);
			context.LoadingRG = &context.LoadingScene->RenderGroups[EnumToInt(context.RGType)];

			material.Albedo = LoadTexture(context, mat.base_color_texture.texture);
			material.MetallicRoughness = LoadTexture(context, mat.metallic_roughness_texture.texture);
			material.Normal = LoadTexture(context, materialData->normal_texture.texture, ColorUNORM(0.5f, 0.5f, 1.0f, 1.0f));

			return context.LoadingRG->AddMaterial(*context.GfxContext, material);
		}

		LoadedObject LoadObject(LoadingContext& context, cgltf_primitive* objectData)
		{
			LoadedObject object{};
			object.MaterialIndex = LoadMaterial(context, objectData->material);
			object.RenderGroup = context.RGType;
			object.MeshIndex = LoadMesh(context, objectData);
			object.BoundingVolume = CalculateBoundingSphere(objectData);
			return object;
		}

		DirectX::XMFLOAT4X4 CalcBaseTransform(cgltf_node* nodeData)
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
			for (int32_t i = hierarchy.size()-1; i >= 0; i--)
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

		std::vector<LoadedObject> LoadNode(LoadingContext& context, cgltf_node* nodeData, LoadedScene& sceneRef)
		{
			std::vector<LoadedObject> objects;
			if (nodeData->mesh)
			{
				uint32_t entityIndex = sceneRef.Entities.size();

				Entity e;
				e.BaseTransform = CalcBaseTransform(nodeData);
				sceneRef.Entities.push_back(e);

				for (size_t i = 0; i < nodeData->mesh->primitives_count; i++)
				{
					LoadedObject obj = LoadObject(context, nodeData->mesh->primitives + i);
					obj.EntityIndex = entityIndex;
					sceneRef.Objects.push_back(obj);
				}
			}
			return objects;
		}
	}

	LoadedScene Load(const std::string& path)
	{
		LoadedScene scene;

		const std::string& ext = PathUtility::GetFileExtension(path);
		if (ext != "gltf")
		{
			ASSERT(0, "[SceneLoading] For now we only support glTF 3D format.");
			return scene;
		}

		LoadingContext context{};
		context.RelativePath = PathUtility::GetPathWitoutFile(path);
		context.GfxContext = &Device::Get()->GetContext();
		context.LoadingScene = MainSceneGraph;

		cgltf_options options = {};
		cgltf_data* data = NULL;
		CGTF_CALL(cgltf_parse_file(&options, path.c_str(), &data));
		CGTF_CALL(cgltf_load_buffers(&options, data, path.c_str()));

		for (size_t i = 0; i < data->nodes_count; i++)
		{
			std::vector<LoadedObject> objects = LoadNode(context, data->nodes + i, scene);
		}

		cgltf_free(data);

		return scene;
	}

	void AddDraws(LoadedScene scene, Entity entity)
	{
		GraphicsContext& context = Device::Get()->GetContext();

		std::unordered_map<uint32_t, uint32_t> entityIndexMap;
		for (uint32_t i = 0; i < scene.Entities.size(); i++)
		{
			Entity e = scene.Entities[i];

			e.Position = entity.Position;
			e.Scale = entity.Scale;
			e.Rotation = entity.Rotation;

			uint32_t entityIndex = MainSceneGraph->AddEntity(context, e);
			entityIndexMap[i] = entityIndex;
		}

		for (const LoadedObject& obj : scene.Objects)
		{
			RenderGroup& rg = MainSceneGraph->RenderGroups[EnumToInt(obj.RenderGroup)];
			rg.AddDraw(Device::Get()->GetContext(), obj.MaterialIndex, obj.MeshIndex, entityIndexMap[obj.EntityIndex], obj.BoundingVolume);
		}
	}
}