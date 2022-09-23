#pragma once

#include "Common.h"

struct Buffer;
struct Texture;
struct GraphicsContext;

struct cgltf_node;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_texture;

#undef OPAQUE

namespace ModelLoading
{
	struct Vertex
	{
		Float3 Position{ 0.0f, 0.0f, 0.0f };
		Float2 Texcoord{ 0.0f, 0.0f };
		Float3 Normal{ 0.0f, 0.0f, 0.0f };
		Float4 Tangent{ 0.0f, 0.0f, 0.0f, 0.0f };
	};

	struct BoundingSphere
	{
		Float3 Center{0.0f, 0.0f, 0.0f};
		float Radius = 1.0f;
	};

	struct SceneObject
	{
		enum {OPAQUE, ALPHA_DISCARD, ALPHA_BLEND};

		BoundingSphere BoundingVolume;
		DirectX::XMFLOAT4X4 ModelToWorld;

		Buffer* Vertices = nullptr;
		Buffer* Indices = nullptr;

		uint32_t BlendMode = OPAQUE;

		Float3 AlbedoFactor{ 1.0f, 1.0f, 1.0f };
		float MetallicFactor = 1.0f;
		float RoughnessFactor = 1.0f;

		Texture* Albedo = nullptr;
		Texture* Normal = nullptr;
		Texture* MetallicRoughness = nullptr;
	};

	class Loader
	{
	public:
		Loader(GraphicsContext& context) : m_Context(context) {}

		std::vector<SceneObject> Load(const std::string& path);

	private:
		void LoadNode(cgltf_node* nodeData);
		void LoadObject(cgltf_primitive* objectData);
		void LoadMesh(cgltf_primitive* meshData);
		void LoadMaterial(cgltf_material* materialData);
		Texture* LoadTexture(cgltf_texture* textureData, ColorUNORM defaultColor = {0.0f, 0.0f, 0.0f, 0.0f});

	private:
		GraphicsContext& m_Context;
		uint32_t m_TextureNumMips = 1;
		bool m_LoadDefaultTextures = false;

		std::vector<SceneObject> m_Scene;
		SceneObject m_Object;
		std::string m_DirectoryPath;
	};

	void Free(SceneObject& sceneObject);
	void Free(std::vector<SceneObject>& scene);
}