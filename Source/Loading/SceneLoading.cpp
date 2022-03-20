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

namespace SceneLoading
{
	namespace
	{
		struct LoadingContext
		{
			std::string RelativePath;
			ID3D11DeviceContext* GfxContext;
		};

		void* GetBufferData(cgltf_accessor* accessor)
		{
			unsigned char* buffer = (unsigned char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset;
			void* data = buffer + accessor->offset;
			return data;
		}

		BufferID GetIndices(cgltf_accessor* indexAccessor)
		{
			ASSERT(indexAccessor, "[SceneLoading] Trying to read indices from empty accessor");
			ASSERT(indexAccessor->type == cgltf_type_scalar, "[SceneLoading] Indices of a mesh arent scalar.");
			
			void* indexData = GetBufferData(indexAccessor);
			uint32_t stride = cgltf_component_size(indexAccessor->component_type);
			return GFX::CreateIndexBuffer(indexAccessor->count * stride, stride, indexData);
		}
		
		template<typename T>
		BufferID GetEmptyVB(unsigned int numVertices)
		{
			void* vbData = calloc(numVertices, sizeof(T));
			BufferID vb = GFX::CreateVertexBuffer<T>(numVertices, (T*) vbData);
			free(vbData);
			return vb;
		}
		
		template<typename T, cgltf_type TYPE, cgltf_component_type COMPONENT_TYPE>
		BufferID GetVB(cgltf_attribute* vertexAttribute)
		{
			ASSERT(vertexAttribute->data->type == TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->type == TYPE");
			ASSERT(vertexAttribute->data->component_type == COMPONENT_TYPE, "[SceneLoading] ASSERT FAILED: attributeAccessor->component_type == COMPONENT_TYPE");
		
			T* attributeData = (T*)GetBufferData(vertexAttribute->data);
			return GFX::CreateVertexBuffer<T>(vertexAttribute->data->count, attributeData);
		}

		Mesh LoadMesh(const LoadingContext& context, cgltf_primitive* meshData)
		{
			ASSERT(meshData->type == cgltf_primitive_type_triangles, "[SceneLoading] Scene contains quad meshes. We are supporting just triangle meshes.");

			Mesh mesh;

			for (size_t i = 0; i < meshData->attributes_count; i++)
			{
				cgltf_attribute* vertexAttribute = (meshData->attributes + i);
				switch (vertexAttribute->type)
				{
				case cgltf_attribute_type_position:
					mesh.Position = GetVB<Float3, cgltf_type_vec3, cgltf_component_type_r_32f>(vertexAttribute);
					break;
				case cgltf_attribute_type_texcoord:
					mesh.UV = GetVB<Float2, cgltf_type_vec2, cgltf_component_type_r_32f>(vertexAttribute);
					break;
				case cgltf_attribute_type_normal:
					mesh.Normal = GetVB<Float3, cgltf_type_vec3, cgltf_component_type_r_32f>(vertexAttribute);
					break;
				case cgltf_attribute_type_tangent:
					mesh.Tangent = GetVB<Float4, cgltf_type_vec4, cgltf_component_type_r_32f>(vertexAttribute);
					break;
				}
			}

			unsigned int vertCount = meshData->attributes->data->count;
			if (!mesh.Position.Valid()) mesh.Position = GetEmptyVB<Float3>(vertCount);
			if (!mesh.UV.Valid()) mesh.UV = GetEmptyVB<Float2>(vertCount);
			if (!mesh.Normal.Valid()) mesh.Normal = GetEmptyVB<Float3>(vertCount);
			if (!mesh.Tangent.Valid()) mesh.Tangent = GetEmptyVB<Float4>(vertCount);

			mesh.Indices = GetIndices(meshData->indices);

			return mesh;
		}

		uint32_t AddTexture(const LoadingContext& context, TextureID texture)
		{
			static TextureID stagingTexture;
			if (!stagingTexture.Valid()) stagingTexture = GFX::CreateTexture(MainSceneGraph.TEXTURE_SIZE, MainSceneGraph.TEXTURE_SIZE, RCF_Bind_RTV | RCF_GenerateMips | RCF_Bind_SRV, MainSceneGraph.TEXTURE_MIPS);

			const Texture& stagingTex = GFX::Storage::GetTexture(stagingTexture);
			
			// Resize texture and generate mips
			ID3D11DeviceContext* c = context.GfxContext;

			GFX::Cmd::MarkerBegin(c, "CopyTexture");
			GFX::Cmd::SetupStaticSamplers<PS>(c);
			GFX::Cmd::BindShader(c, Device::Get()->GetCopyShader());
			c->OMSetRenderTargets(1, stagingTex.RTV.GetAddressOf(), nullptr);
			GFX::Cmd::SetViewport(c, { (float) MainSceneGraph.TEXTURE_SIZE, (float) MainSceneGraph.TEXTURE_SIZE });
			GFX::Cmd::BindVertexBuffer(c, Device::Get()->GetQuadBuffer());
			GFX::Cmd::BindSRV<PS>(c, texture, 0);
			c->Draw(6, 0);
			GFX::Cmd::MarkerEnd(c);
			
			c->GenerateMips(stagingTex.SRV.Get());

			GFX::Storage::Free(texture);

			// Copy to the array
			const Texture& textureArray = GFX::Storage::GetTexture(MainSceneGraph.Textures);
			const uint32_t textureIndex = MainSceneGraph.NextTextureIndex++;
			ASSERT(textureIndex < MainSceneGraph.MAX_TEXTURES, "textureIndex < MainSceneGraph.MAX_TEXTURES");
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
			return MainSceneGraph.CreateDrawable(context.GfxContext, LoadMaterial(context, meshData->material), LoadMesh(context, meshData));
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

		cgltf_options options = {};
		cgltf_data* data = NULL;
		CGTF_CALL(cgltf_parse_file(&options, path.c_str(), &data));
		CGTF_CALL(cgltf_load_buffers(&options, data, path.c_str()));

		for (size_t i = 0; i < data->meshes_count; i++)
		{
			cgltf_mesh* meshData = (data->meshes + i);
			for (size_t j = 0; j < meshData->primitives_count; j++)
			{
				Drawable d = LoadDrawable(context, meshData->primitives + j);
				entityOut.Drawables.Add(d);
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

			std::vector<Drawable> drawables;
			for (size_t i = 0; i < data->meshes_count; i++)
			{
				cgltf_mesh* meshData = (data->meshes + i);
				for (size_t j = 0; j < meshData->primitives_count; j++)
				{
					if (ShouldStop()) break; // Something requested stop

					Drawable d = LoadDrawable(loadingContext, meshData->primitives + j);
					drawables.push_back(d);

					if (drawables.size() >= BATCH_SIZE)
					{
						GFX::Cmd::SubmitDeferredContext(context);
						GFX::Cmd::ResetContext(context);
						m_Entity.Drawables.AddAll(drawables);
						drawables.clear();
					}
					
				}
			}
			m_Entity.Drawables.AddAll(drawables);
			cgltf_free(data);
		}

	private:
		std::string m_Path;
		Entity& m_Entity;

		static constexpr uint32_t BATCH_SIZE = 4;
	};

	void LoadEntityInBackground(const std::string& path, Entity& entityOut)
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
}