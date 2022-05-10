#include "SceneLoading.h"

#pragma warning (disable : 4996)
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "Loading/LoadingThread.h"
#include "Render/Device.h"
#include "Render/Buffer.h"
#include "Render/Texture.h"
#include "Utility/PathUtility.h"
#include "System/ApplicationConfiguration.h"

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
			ID3D11DeviceContext* GfxContext;
			Entity* LoadingEntity;
			SceneGraph* LoadingScene;
		};

		void* GetBufferData(cgltf_accessor* accessor)
		{
			unsigned char* buffer = (unsigned char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset;
			void* data = buffer + accessor->offset;
			return data;
		}

		void UpdateIB(const LoadingContext& context, cgltf_accessor* indexAccessor, std::vector<uint32_t>& buffer, const Mesh& mesh)
		{
			ASSERT(indexAccessor, "[SceneLoading] Trying to read indices from empty accessor");
			ASSERT(indexAccessor->type == cgltf_type_scalar, "[SceneLoading] Indices of a mesh arent scalar.");
			ASSERT(indexAccessor->component_type == cgltf_component_type_r_16u, "[SceneLoading] Indices must be uint16.");
			
			uint16_t* indexData = (uint16_t*) GetBufferData(indexAccessor);
			for (size_t i = 0; i < indexAccessor->count; i++)
			{
				buffer[mesh.IndexOffset + i] = indexData[i] + mesh.VertOffset;
			}
		}

		template<typename T, cgltf_type TYPE, cgltf_component_type COMPONENT_TYPE>
		void UpdateVB(const LoadingContext& context, cgltf_attribute* vertexAttribute, BufferID buffer, uint32_t offset)
		{
			ASSERT(vertexAttribute->data->type == TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->type == TYPE");
			ASSERT(vertexAttribute->data->component_type == COMPONENT_TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->component_type == COMPONENT_TYPE");

			T* attributeData = (T*)GetBufferData(vertexAttribute->data);

			GFX::Cmd::UploadToBuffer(context.GfxContext, buffer, offset * sizeof(T), attributeData, 0, vertexAttribute->data->count * sizeof(T));
		}

		Mesh LoadMesh(const LoadingContext& context, MeshStorage& meshStorage, cgltf_primitive* meshData)
		{
			ASSERT(meshData->type == cgltf_primitive_type_triangles, "[SceneLoading] Scene contains quad meshes. We are supporting just triangle meshes.");

			Mesh mesh;
			mesh.VertCount = meshData->attributes[0].data->count;
			mesh.IndexCount = meshData->indices->count;

			MeshStorage::Allocation alloc = meshStorage.Allocate(context.GfxContext, mesh.VertCount, mesh.IndexCount);

			mesh.VertOffset = alloc.VertexOffset;
			mesh.IndexOffset = alloc.IndexOffset;

			for (size_t i = 0; i < meshData->attributes_count; i++)
			{
				cgltf_attribute* vertexAttribute = (meshData->attributes + i);
				switch (vertexAttribute->type)
				{
				case cgltf_attribute_type_position:
					UpdateVB<Float3, cgltf_type_vec3, cgltf_component_type_r_32f>(context, vertexAttribute, meshStorage.GetPositions(), mesh.VertOffset);
					break;
				case cgltf_attribute_type_texcoord:
					UpdateVB<Float2, cgltf_type_vec2, cgltf_component_type_r_32f>(context, vertexAttribute, meshStorage.GetTexcoords(), mesh.VertOffset);
					break;
				case cgltf_attribute_type_normal:
					UpdateVB<Float3, cgltf_type_vec3, cgltf_component_type_r_32f>(context, vertexAttribute, meshStorage.GetNormals(), mesh.VertOffset);
					break;
				case cgltf_attribute_type_tangent:
					UpdateVB<Float4, cgltf_type_vec4, cgltf_component_type_r_32f>(context, vertexAttribute, meshStorage.GetTangents(), mesh.VertOffset);
					break;
				}
			}

			// TODO: memset 0 to missing attributes

			UpdateIB(context, meshData->indices, meshStorage.GetIndexBuffer(), mesh);

			return mesh;
		}

		BoundingSphere CalculateBoundingSphere(cgltf_primitive* meshData)
		{
			Float3* vertexData = nullptr;
			uint32_t vertexCount = meshData->attributes[0].data->count;

			for (size_t i = 0; i < meshData->attributes_count; i++)
			{
				cgltf_attribute* vertexAttribute = (meshData->attributes + i);
				switch (vertexAttribute->type)
				{
				case cgltf_attribute_type_position:
					ASSERT(vertexAttribute->data->type == cgltf_type_vec3, "[SceneLoading] ASSERT FAILED: attributeAccessor->type == TYPE");
					ASSERT(vertexAttribute->data->component_type == cgltf_component_type_r_32f, "[SceneLoading] ASSERT FAILED: attributeAccessor->component_type == COMPONENT_TYPE");
					vertexData = (Float3*) GetBufferData(vertexAttribute->data);
					break;
				}
			}

			ASSERT(vertexData, "[SceneLoading] ASSERT FAILED: vertexData");

			static constexpr float MAX_FLOAT = std::numeric_limits<float>::max();
			static constexpr float MIN_FLOAT = std::numeric_limits<float>::min();

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
			bs.Radius = Float3(DirectX::XMVector3LengthEst(minAABB - maxAABB)).x;

			return bs;
		}

		uint32_t AddTexture(const LoadingContext& context, TextureID texture)
		{
			static TextureID stagingTexture;
			if (!stagingTexture.Valid()) stagingTexture = GFX::CreateTexture(SceneGraph::TEXTURE_SIZE, SceneGraph::TEXTURE_SIZE, RCF_Bind_RTV | RCF_GenerateMips | RCF_Bind_SRV, SceneGraph::TEXTURE_MIPS);

			const Texture& stagingTex = GFX::Storage::GetTexture(stagingTexture);
			
			// Resize texture and generate mips
			ID3D11DeviceContext* c = context.GfxContext;

			GFX::Cmd::MarkerBegin(c, "CopyTexture");
			GFX::Cmd::SetupStaticSamplers<PS>(c);
			GFX::Cmd::BindShader(c, Device::Get()->GetCopyShader());
			c->OMSetRenderTargets(1, stagingTex.RTV.GetAddressOf(), nullptr);
			GFX::Cmd::SetViewport(c, { (float) SceneGraph::TEXTURE_SIZE, (float) SceneGraph::TEXTURE_SIZE });
			GFX::Cmd::BindVertexBuffer(c, Device::Get()->GetQuadBuffer());
			GFX::Cmd::BindSRV<PS>(c, texture, 0);
			c->Draw(6, 0);
			GFX::Cmd::MarkerEnd(c);
			
			c->GenerateMips(stagingTex.SRV.Get());

			GFX::Storage::Free(texture);

			// Copy to the array
			const Texture& textureArray = GFX::Storage::GetTexture(context.LoadingScene->Textures);
			const uint32_t textureIndex = context.LoadingScene->NextTextureIndex++;
			ASSERT(textureIndex < SceneGraph::MAX_TEXTURES, "textureIndex < SceneGraph::MAX_TEXTURES");
			ASSERT(stagingTex.Format == textureArray.Format, "stagingTex.Format == tex.Format");

			for (uint32_t mip = 0; mip < textureArray.NumMips; mip++)
			{
				uint32_t srcSubresource = D3D11CalcSubresource(mip, 0, stagingTex.NumMips);
				uint32_t dstSubresource = D3D11CalcSubresource(mip, textureIndex, textureArray.NumMips);
				c->CopySubresourceRegion(textureArray.Handle.Get(), dstSubresource, 0, 0, 0, stagingTex.Handle.Get(), srcSubresource, nullptr);
			}

			return textureIndex;
		}

		uint32_t LoadTexture(const LoadingContext& context, cgltf_texture* texture, ColorUNORM defaultColor = {1.0f, 1.0f, 1.0f, 1.0f})
		{
			TextureID tex;
			if (texture)
			{
				const std::string textureURI = texture->image->uri;
				const std::string texutrePath = context.RelativePath + "/" + textureURI;
				tex = GFX::LoadTexture(context.GfxContext, texutrePath, RCF_Bind_SRV);
			}
			else
			{
				tex = GFX::CreateTexture(1, 1, RCF_Bind_SRV, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &defaultColor);
			}
			
			return AddTexture(context, tex);
		}

		static Float3 ToFloat3(cgltf_float color[3])
		{
			return Float3{ color[0], color[1], color[2] };
		}

		Material LoadMaterial(const LoadingContext& context, cgltf_material* materialData)
		{
			ASSERT(materialData->has_pbr_metallic_roughness, "[SceneLoading] Every material must have a base color texture!");

			cgltf_pbr_metallic_roughness& mat = materialData->pbr_metallic_roughness;

			Material material;
			material.UseBlend = materialData->alpha_mode == cgltf_alpha_mode_blend;
			material.UseAlphaDiscard = materialData->alpha_mode == cgltf_alpha_mode_mask;
			material.Albedo = LoadTexture(context, mat.base_color_texture.texture);
			material.AlbedoFactor = ToFloat3(mat.base_color_factor);
			material.MetallicRoughness = LoadTexture(context, mat.metallic_roughness_texture.texture);
			material.MetallicFactor = mat.metallic_factor;
			material.RoughnessFactor = mat.roughness_factor;
			material.Normal = LoadTexture(context, materialData->normal_texture.texture, ColorUNORM(0.5f, 0.5f, 1.0f, 1.0f));
			return material;
		}

		Drawable LoadDrawable(const LoadingContext& context, cgltf_primitive* meshData)
		{
			Material material = LoadMaterial(context, meshData->material);
			MeshStorage& meshStorage = context.LoadingScene->Geometries;

			Mesh mesh = LoadMesh(context, meshStorage, meshData);
			BoundingSphere boundingSphere = CalculateBoundingSphere(meshData);
			Drawable drawable = context.LoadingScene->CreateDrawable(context.GfxContext, material, mesh, boundingSphere, *context.LoadingEntity);

			std::vector<uint32_t> drawIndexData;
			uint32_t drawIndex = drawable.DrawableIndex;
			drawIndexData.resize(mesh.VertCount);
			for (uint32_t i = 0; i < mesh.VertCount; i++)
			{
				drawIndexData[i] = drawIndex;
			}
			GFX::Cmd::UploadToBuffer(context.GfxContext, meshStorage.GetDrawableIndexes(), mesh.VertOffset * MeshStorage::GetDrawableIndexStride(), drawIndexData.data(), 0, drawIndexData.size() * MeshStorage::GetDrawableIndexStride());
			
			return drawable;
		}
	}
	
	void LoadEntity(const std::string& path, Entity& entityOut)
	{
		const std::string& ext = PathUtility::GetFileExtension(path);
		if (ext != "gltf")
		{
			ASSERT(0, "[SceneLoading] For now we only support glTF 3D format.");
			return;
		}

		LoadingContext context{};
		context.RelativePath = PathUtility::GetPathWitoutFile(path);
		context.GfxContext = Device::Get()->GetContext();
		context.LoadingEntity = &entityOut;
		context.LoadingScene = &MainSceneGraph;

		cgltf_options options = {};
		cgltf_data* data = NULL;
		CGTF_CALL(cgltf_parse_file(&options, path.c_str(), &data));
		CGTF_CALL(cgltf_load_buffers(&options, data, path.c_str()));

		for (size_t i = 0; i < data->meshes_count; i++)
		{
			cgltf_mesh* meshData = (data->meshes + i);
			for (size_t j = 0; j < meshData->primitives_count; j++)
			{
				LoadDrawable(context, meshData->primitives + j);
			}
		}
		cgltf_free(data);
	}

	class EntityLoadingTask : public LoadingTask
	{
	public:
		EntityLoadingTask(const std::string& path, Entity& entity) :
			m_Path(path),
			m_Entity(entity) {}


		void Run(ID3D11DeviceContext* context) override
		{
			const std::string& ext = PathUtility::GetFileExtension(m_Path);
			if (ext != "gltf")
			{
				ASSERT(0, "[SceneLoading] For now we only support glTF 3D format.");
				return;
			}

			cgltf_options options = {};
			cgltf_data* data = NULL;
			CGTF_CALL(cgltf_parse_file(&options, m_Path.c_str(), &data));
			CGTF_CALL(cgltf_load_buffers(&options, data, m_Path.c_str()));

			LoadingContext loadingContext{};
			loadingContext.RelativePath = PathUtility::GetPathWitoutFile(m_Path);
			loadingContext.GfxContext = context;
			loadingContext.LoadingEntity = &m_Entity;
			loadingContext.LoadingScene = &MainSceneGraph;

			uint32_t pendingDrawables = 0;
			for (size_t i = 0; i < data->meshes_count; i++)
			{
				cgltf_mesh* meshData = (data->meshes + i);
				for (size_t j = 0; j < meshData->primitives_count; j++)
				{
					if (ShouldStop()) break; // Something requested stop

					LoadDrawable(loadingContext, meshData->primitives + j);
					pendingDrawables++;

					if (pendingDrawables >= BATCH_SIZE)
					{
						GFX::Cmd::SubmitDeferredContext(context);
						GFX::Cmd::ResetContext(context);
						pendingDrawables = 0;
					}
					
				}
			}

			GFX::Cmd::SubmitDeferredContext(context);
			GFX::Cmd::ResetContext(context);

			cgltf_free(data);
		}

	private:
		std::string m_Path;
		Entity& m_Entity;

		static constexpr uint32_t BATCH_SIZE = 4;
	};

	void LoadEntityInBackground(const std::string& path, Entity& entityOut)
	{
		static constexpr bool ENABLE_BG_LOADING = false;
		if constexpr (ENABLE_BG_LOADING)
		{
			if (AppConfig.Settings.contains("NO_BG_LOADING"))
			{
				LoadEntity(path, entityOut);
			}
			else
			{
				LoadingThread::Get()->Submit(new EntityLoadingTask(path, entityOut));
			}
		}
		else
		{
			LoadEntity(path, entityOut);
		}
	}
}