#pragma once

#include <vector>

#include "Common.h"
#include "Render/ResourceID.h"
#include "Utility/Multithreading.h"

struct ID3D11DeviceContext;

struct Material
{
	bool UseBlend = false;
	bool UseAlphaDiscard = false;
	Float3 AlbedoFactor = { 1.0f, 1.0f, 1.0f };
	Float3 FresnelR0 = { 0.05f, 0.05f, 0.05f };
	float MetallicFactor = 1.0f;
	float RoughnessFactor = 1.0f;

	uint32_t Albedo;
	uint32_t MetallicRoughness;
	uint32_t Normal;
};

struct Mesh
{
	BufferID Position; // Float3
	BufferID UV; // Float2
	BufferID Normal; // Float3
	BufferID Tangent; // Float4
	BufferID Indices; // uint16_t
};

struct Drawable
{
	void UpdateBuffer(ID3D11DeviceContext* context);

	uint32_t MaterialIndex;
	uint32_t MeshIndex;
};

struct Entity
{
	void UpdateBuffer(ID3D11DeviceContext* context);

	Float3 Position = Float3(0.0f, 0.0f, 0.0f);
	Float3 Scale = Float3(1.0f, 1.0f, 1.0f);

	uint32_t Index;
	MTR::MutexVector<Drawable> Drawables;
};

struct Camera
{
	static void RotToAxis(Float3 rot, Float3& forward, Float3& up, Float3& right);

	Camera(Float3 position, Float3 rotation, float fov);
	void UpdateBuffer(ID3D11DeviceContext* context);

	float FOV;
	Float3 Position;
	Float3 Rotation; // (Pitch, Yaw, Roll)

	DirectX::XMMATRIX WorldToView;

	BufferID CameraBuffer;
};

enum LightType : uint32_t
{
	LT_Invalid = 0,
	LT_Directional = 1,
	LT_Point = 2,
	LT_Spot = 3,
	LT_Ambient = 4,
};

struct Light
{
	static Light CreateDirectional(Float3 direction, Float3 color);
	static Light CreateAmbient(Float3 color);
	static Light CreatePoint(Float3 position, Float3 color, Float2 falloff);

	LightType Type = LT_Invalid;
	Float3 Position = { 0.0f, 0.0f, 0.0f };		// Point
	Float3 Strength = { 0.0f, 0.0f, 0.0f };		// Dir/Spot/Point/Ambient
	Float2 Falloff = { 0.0f, 0.0f };			// Point/Spot (Start, End)
	Float3 Direction = { 0.0f, 0.0f, 0.0f };	// Dir/Spot
	float SpotPower = 0.0f;						// Spot
};

namespace ElementBufferHelp
{
	BufferID CreateBuffer(uint32_t numElements, uint32_t stride);
	void DeleteBuffer(BufferID buffer);
}

template<typename T>
class ElementBuffer
{
public:
	ElementBuffer(uint32_t maxElements, uint32_t gpuBufferStride):
		m_MaxElements(maxElements),
		m_GpuStride(gpuBufferStride)
	{ }

	void Initialize()
	{
		m_Storage.resize(m_MaxElements);
		if(m_GpuStride != 0) m_Buffer = ElementBufferHelp::CreateBuffer(m_MaxElements, m_GpuStride);
	}

	~ElementBuffer()
	{
		if (m_GpuStride != 0) ElementBufferHelp::DeleteBuffer(m_Buffer);
	}

	T& operator [] (uint32_t index) { return m_Storage[index]; }
	size_t GetSize() const { return m_NextIndex; }
	size_t Next() 
	{ 
		size_t index = m_NextIndex++;
		ASSERT(index < m_MaxElements, "index < m_MaxElements");
		return index; 
	}
	BufferID GetBuffer() const { ASSERT(m_GpuStride != 0, "m_GpuStride != 0");  return m_Buffer; }

private:
	uint32_t m_MaxElements;
	uint32_t m_GpuStride;
	std::atomic<uint32_t> m_NextIndex;

	std::vector<T> m_Storage;
	BufferID m_Buffer;
};

struct SceneGraph
{
	static constexpr uint32_t MAX_ENTITIES = 100;
	static constexpr uint32_t MAX_DRAWABLES = 100000;

	SceneGraph();

	void UpdateDrawables(ID3D11DeviceContext* context);
	void UpdateRenderData(ID3D11DeviceContext* context);
	Entity& CreateEntity(ID3D11DeviceContext* context, Float3 position = { 0.0f, 0.0f, 0.0f }, Float3 scale = { 1.0f, 1.0f, 1.0f });
	Drawable CreateDrawable(ID3D11DeviceContext* context, const Material material, const Mesh mesh);

	Camera MainCamera{ {0.0f, 2.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 75.0f };
	ElementBuffer<Entity> Entities;
	ElementBuffer<Material> Materials;
	ElementBuffer<Mesh> Meshes;
	std::vector<Light> Lights;

	BufferID LightsBuffer;
	BufferID WorldToLightClip;
	TextureID ShadowMapTexture;

	static constexpr uint32_t MAX_TEXTURES = 500;
	static constexpr uint32_t TEXTURE_MIPS = 8;
	static constexpr uint32_t TEXTURE_SIZE = 1024;
	std::atomic<uint32_t> NextTextureIndex = 0;
	TextureID Textures;

	MTR::MutexVector<Drawable> DrawablesToUpdate;
};

extern SceneGraph MainSceneGraph;