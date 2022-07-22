#pragma once

#include <vector>

#include <Engine/Common.h>
#include <Engine/Utility/Multithreading.h>

struct GraphicsState;
struct GraphicsContext;
struct Buffer;
struct Texture;

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

struct RenderGroup;

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

	void UpdateBuffer(GraphicsContext& context, RenderGroup& renderGroup);
};

struct Mesh
{
	uint32_t MeshIndex = 0;

	uint32_t VertCount;
	uint32_t IndexCount;

	uint32_t VertOffset;
	uint32_t IndexOffset;
	
	std::vector<DirectX::CullData> MeshletCullData;

	void UpdateBuffer(GraphicsContext& context, RenderGroup& renderGroup);
};

struct Drawable
{
	static constexpr uint32_t InvalidIndex = static_cast<uint32_t>(-1);

	uint32_t DrawableIndex = InvalidIndex;

	uint32_t EntityIndex;
	uint32_t MaterialIndex;
	uint32_t MeshIndex;

	BoundingSphere BoundingVolume;

	void UpdateBuffer(GraphicsContext& context, RenderGroup& renderGroup);
};

struct Entity
{
	uint32_t EntityIndex;

	DirectX::XMFLOAT4X4 BaseTransform = XMUtility::ToXMFloat4x4(DirectX::XMMatrixIdentity());
	Float3 Position = Float3(0.0f, 0.0f, 0.0f);
	Float3 Scale = Float3(1.0f, 1.0f, 1.0f);
	Float3 Rotation = Float3(0.0f, 0.0f, 0.0f);

	BoundingSphere GetBoundingVolume(BoundingSphere bv) const;

	void UpdateBuffer(GraphicsContext& context);
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

	void UpdateBuffer(GraphicsContext& context);
};

struct Camera
{
	enum class CameraType
	{
		Ortho,
		Perspective,
	};

	struct CameraRenderData
	{
		DirectX::XMFLOAT4X4 WorldToView;
		DirectX::XMFLOAT4X4 ViewToClip;
		DirectX::XMFLOAT4X4 ClipToWorld;
		DirectX::XMFLOAT3A Position;
		DirectX::XMFLOAT2A Jitter;
	};

	struct CameraTransform
	{
		Float3 Position{ 0.0f, 2.0f, 0.0f };
		Float3 Rotation{ 0.0f, 0.0f, 0.0f }; // (Pitch, Yaw, Roll)
		Float3 Forward{ 1.0f, 0.0f, 0.0f };
		Float3 Up{ 0.0f, 1.0f, 0.0f };
		Float3 Right{ 0.0f, 0.0f, 1.0f };
	};
	
	static Camera CreatePerspective(float fov, float aspect, float znear, float zfar);
	static Camera CreateOrtho(float rectWidth, float rectHeight, float znear, float zfar);

	void FrameUpdate(GraphicsContext& context);
	void UpdateRenderData();
	void UpdateRenderDataForTransform(CameraTransform& transform, CameraRenderData& destData);

	CameraType Type;
	float ZFar;
	float ZNear;

	// Perspective data
	float AspectRatio;
	float FOV;

	// Ortho data
	float RectWidth;
	float RectHeight;
	
	bool UseRotation = true; // Uses rotation (pitch,yaw,roll) for calculation forward vector
	CameraTransform NextTransform;
	CameraTransform CurrentTranform;
	CameraTransform LastTransform;

	bool UseJitter = false;
	uint8_t JitterIndex = 0;
	Float2 Jitter[16];

	DirectX::XMMATRIX WorldToView = DirectX::XMMatrixIdentity();

	CameraRenderData CameraData;
	CameraRenderData LastCameraData;

	ViewFrustum CameraFrustum;
};

namespace ElementBufferHelp
{
	Buffer* CreateBuffer(uint32_t numElements, uint32_t stride, const std::string& debugName);
	void DeleteBuffer(Buffer* buffer);
}

template<typename T>
class ElementBuffer
{
public:
	ElementBuffer(uint32_t maxElements, uint32_t gpuBufferStride):
		m_MaxElements(maxElements),
		m_GpuStride(gpuBufferStride)
	{ }

	void Initialize(const std::string& debugName = "ElementBuffer::Unnamed")
	{
		m_Storage.resize(m_MaxElements);
		if(m_GpuStride != 0) m_Buffer = ElementBufferHelp::CreateBuffer(m_MaxElements, m_GpuStride, debugName);
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
	Buffer* GetBuffer() const { ASSERT(m_GpuStride != 0, "m_GpuStride != 0");  return m_Buffer; }

private:
	uint32_t m_MaxElements;
	uint32_t m_GpuStride;
	std::atomic<uint32_t> m_NextIndex;

	std::vector<T> m_Storage;
	Buffer* m_Buffer;
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

	MeshStorage();
	~MeshStorage();

	void Initialize();

	Allocation Allocate(GraphicsContext& context, uint32_t vertexCount, uint32_t indexCount);

	Buffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }
	Buffer* GetIndexBuffer() const { return m_IndexBuffer.get(); }

	uint32_t GetVertexCount() const { return m_VertexCount; }
	uint32_t GetIndexCount() const { return m_IndexCount; }

	static constexpr uint8_t GetVertexBufferStride()	{ return sizeof(Vertex); }
	static constexpr uint8_t GetIndexBufferStride()		{ return sizeof(uint32_t); }

private:
	std::atomic<uint32_t> m_VertexCount = 0;
	std::atomic<uint32_t> m_IndexCount = 0;

	ScopedRef<Buffer> m_VertexBuffer;
	ScopedRef<Buffer> m_IndexBuffer;
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

	TextureStorage();
	~TextureStorage();

	void Initialize();
	Allocation AddTexture(GraphicsContext& context, Texture* texture);
	Allocation AllocTexture(GraphicsContext& context);
	void UpdateTexture(GraphicsContext& context, Allocation alloc,  Texture* texture);
	Texture* GetBuffer() const { return m_Data.get(); }

private:
	ScopedRef<Texture> m_Data;
	ScopedRef<Texture> m_StagingTexture;
	std::atomic<uint32_t> m_NextAllocation = 0;
};

// Group of data that can be rendered at once
struct RenderGroup
{
	static constexpr uint32_t MAX_DRAWABLES = 200000;

	RenderGroup();
	void Initialize(GraphicsContext& context);
	uint32_t AddMaterial(GraphicsContext& context, Material& material);
	uint32_t AddMesh(GraphicsContext& context, Mesh& mesh);
	void AddDraw(GraphicsContext& context, uint32_t materialIndex, uint32_t meshIndex, uint32_t entityIndex, const BoundingSphere& boundingSphere);

	void SetupPipelineInputs(GraphicsState& state);

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
	static constexpr uint32_t MAX_ENTITIES = 10000;
	static constexpr uint32_t MAX_LIGHTS = 100000;

	struct SceneInfoRenderData
	{
		uint32_t NumLights;
		DirectX::XMFLOAT3 Padding;
		DirectX::XMFLOAT2A ScreenSize;
		float AspectRatio;
	};

	SceneGraph();
	void InitRenderData(GraphicsContext& context);

	void FrameUpdate(GraphicsContext& context);
	uint32_t AddEntity(GraphicsContext& context, Entity entity);

	Light CreateDirectionalLight(GraphicsContext& context, Float3 direction, Float3 color);
	Light CreateAmbientLight(GraphicsContext& context, Float3 color);
	Light CreatePointLight(GraphicsContext& context, Float3 position, Float3 color, Float2 falloff);
	Light CreateLight(GraphicsContext& context, Light light);

	Camera MainCamera;
	Camera ShadowCamera;

	ElementBuffer<Entity> Entities;
	RenderGroup RenderGroups[EnumToInt(RenderGroupType::Count)];
	ElementBuffer<Light> Lights;
	uint32_t DirLightIndex = UINT32_MAX;

	SceneInfoRenderData SceneInfoData;
};

extern SceneGraph* MainSceneGraph;