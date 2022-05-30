#pragma once

#include <vector>

#include "Common.h"
#include "Render/ResourceID.h"
#include "Utility/Multithreading.h"

struct ID3D11DeviceContext;

namespace DirectX
{
	struct CullData;
};

struct BoundingSphere
{
	Float3 Center{ 0.0f, 0.0f, 0.0f };
	float Radius{ 1.0f };
};

// Ax + By + Cz + D
struct FrustumPlane
{
	float A;
	float B;
	float C;
	float D;

	FrustumPlane() = default;

	FrustumPlane(Float3 p0, Float3 p1, Float3 p2)
	{
		const Float3 v = p1 - p0;
		const Float3 u = p2 - p0;
		const Float3 normal = v.Cross(u).Normalize();

		A = normal.x;
		B = normal.y;
		C = normal.z;
		D = -A * p0.x - B * p0.y - C * p0.z;
	}

	float SignedDistance(const Float3& p) const
	{
		return A * p.x + B * p.y + C * p.z + D;
	}
};

struct Camera;

struct ViewFrustum
{
	// Top Bottom Left Right Near Far
	FrustumPlane Planes[6];

	void Update(const Camera& camera);

	bool IsInFrustum(const BoundingSphere& sphere) const
	{
		for (uint32_t i = 0; i < 6; i++)
		{
			const float distance = Planes[i].SignedDistance(sphere.Center);
			if (distance < -sphere.Radius)
				return false;
			
			// Intersects plane
			if (distance < sphere.Radius)
				return true;

		}
		return true;
	}
};

class RenderGroup;

struct Material
{
	uint32_t MaterialIndex = 0;

	bool UseBlend = false;
	bool UseAlphaDiscard = false;
	Float3 AlbedoFactor = { 1.0f, 1.0f, 1.0f };
	Float3 FresnelR0 = { 0.05f, 0.05f, 0.05f };
	float MetallicFactor = 1.0f;
	float RoughnessFactor = 1.0f;

	uint32_t Albedo;
	uint32_t MetallicRoughness;
	uint32_t Normal;

	void UpdateBuffer(ID3D11DeviceContext* context, RenderGroup& renderGroup);
};

struct Mesh
{
	uint32_t MeshIndex = 0;

	uint32_t VertCount;
	uint32_t IndexCount;

	uint32_t VertOffset;
	uint32_t IndexOffset;
	
	std::vector<DirectX::CullData> MeshletCullData;

	void UpdateBuffer(ID3D11DeviceContext* context, RenderGroup& renderGroup);
};

struct Drawable
{
	static constexpr uint32_t InvalidIndex = static_cast<uint32_t>(-1);

	uint32_t DrawableIndex = InvalidIndex;

	uint32_t EntityIndex;
	uint32_t MaterialIndex;
	uint32_t MeshIndex;

	BoundingSphere BoundingVolume;

	void UpdateBuffer(ID3D11DeviceContext* context, RenderGroup& renderGroup);
};

struct Entity
{
	uint32_t EntityIndex;

	Float3 Position = Float3(0.0f, 0.0f, 0.0f);
	Float3 Scale = Float3(1.0f, 1.0f, 1.0f);

	void UpdateBuffer(ID3D11DeviceContext* context);
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
	uint32_t LightIndex = 0;

	LightType Type = LT_Invalid;
	Float3 Position = { 0.0f, 0.0f, 0.0f };		// Point
	Float3 Radiance = { 0.0f, 0.0f, 0.0f };		// Dir/Spot/Point/Ambient
	Float2 Falloff = { 0.0f, 0.0f };			// Point/Spot (Start, End)
	Float3 Direction = { 0.0f, 0.0f, 0.0f };	// Dir/Spot
	float SpotPower = 0.0f;						// Spot

	void UpdateBuffer(ID3D11DeviceContext* context);
};

struct Camera
{
	struct CameraTransform
	{
		Float3 Position;
		Float3 Rotation; // (Pitch, Yaw, Roll)
		Float3 Forward;
		Float3 Up;
		Float3 Right;
	};

	static void RotToAxis(CameraTransform& transform);

	Camera(Float3 position, Float3 rotation, float fov);
	void FrameUpdate(ID3D11DeviceContext* context);
	void UpdateBuffers(ID3D11DeviceContext* context);
	void UpdateBufferForTransform(ID3D11DeviceContext* context, CameraTransform& transform, BufferID& destBuffer);

	float AspectRatio;
	float FOV;
	float ZFar;
	float ZNear;

	CameraTransform NextTransform;
	CameraTransform CurrentTranform;
	CameraTransform LastTransform;

	bool UseJitter = false;
	uint8_t JitterIndex = 0;
	Float2 Jitter[16];

	DirectX::XMMATRIX WorldToView;

	BufferID CameraBuffer;
	BufferID LastFrameCameraBuffer;

	ViewFrustum CameraFrustum;
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

class MeshStorage
{
public:
	struct Vertex
	{
		Float3 Position;
		Float2 Texcoord;
		Float3 Normal;
		Float4 Tangent;
	};

	struct Allocation
	{
		uint32_t VertexOffset;
		uint32_t IndexOffset;
	};

	void Initialize();

	Allocation Allocate(ID3D11DeviceContext* context, uint32_t vertexCount, uint32_t indexCount);

	BufferID GetVertexBuffer() const { return m_VertexBuffer; }
	BufferID GetIndexBuffer() const { return m_IndexBuffer; }

	uint32_t GetVertexCount() const { return m_VertexCount; }
	uint32_t GetIndexCount() const { return m_IndexCount; }

	static constexpr uint8_t GetVertexBufferStride()	{ return sizeof(Vertex); }
	static constexpr uint8_t GetIndexBufferStride()		{ return sizeof(uint32_t); }

private:
	std::atomic<uint32_t> m_VertexCount = 0;
	std::atomic<uint32_t> m_IndexCount = 0;

	BufferID m_VertexBuffer;
	BufferID m_IndexBuffer;
};

class TextureStorage
{
public:
	struct Allocation
	{
		uint32_t TextureIndex;
	};

	static constexpr uint32_t MAX_TEXTURES = 500;
	static constexpr uint32_t TEXTURE_MIPS = 8;
	static constexpr uint32_t TEXTURE_SIZE = 1024;

	void Initialize();
	Allocation AddTexture(ID3D11DeviceContext* context, TextureID texture);
	TextureID GetBuffer() const { return m_Data; }

private:
	TextureID m_Data;
	TextureID m_StagingTexture;
	std::atomic<uint32_t> m_NextAllocation = 0;
};

// Group of data that can be rendered at once
struct RenderGroup
{
	static constexpr uint32_t MAX_DRAWABLES = 100000;

	RenderGroup();
	void Initialize(ID3D11DeviceContext* context);
	uint32_t AddMaterial(ID3D11DeviceContext* context, Material& material);
	uint32_t AddMesh(ID3D11DeviceContext* context, Mesh& mesh);
	void AddDraw(ID3D11DeviceContext* context, uint32_t materialIndex, uint32_t meshIndex, uint32_t entityIndex, const BoundingSphere& boundingSphere);

	ElementBuffer<Material> Materials;
	ElementBuffer<Mesh> Meshes;
	ElementBuffer<Drawable> Drawables;

	TextureStorage TextureData;
	MeshStorage MeshData;

	BitField VisibilityMask;
};

enum class RenderGroupType : uint8_t
{
	Opaque = 0,
	AlphaDiscard = 1,
	Transparent = 2,
	Count,
};

struct SceneGraph
{
	static constexpr uint32_t MAX_ENTITIES = 100;
	static constexpr uint32_t MAX_LIGHTS = 10500;

	SceneGraph();
	void InitRenderData(ID3D11DeviceContext* context);

	void FrameUpdate(ID3D11DeviceContext* context);
	uint32_t AddEntity(ID3D11DeviceContext* context, Entity entity);

	Light CreateDirectionalLight(ID3D11DeviceContext* context, Float3 direction, Float3 color);
	Light CreateAmbientLight(ID3D11DeviceContext* context, Float3 color);
	Light CreatePointLight(ID3D11DeviceContext* context, Float3 position, Float3 color, Float2 falloff);
	Light CreateLight(ID3D11DeviceContext* context, Light light);

	Camera MainCamera{ {0.0f, 2.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 75.0f };

	ElementBuffer<Entity> Entities;
	RenderGroup RenderGroups[EnumToInt(RenderGroupType::Count)];
	ElementBuffer<Light> Lights;

	BufferID SceneInfoBuffer;
};

extern SceneGraph MainSceneGraph;