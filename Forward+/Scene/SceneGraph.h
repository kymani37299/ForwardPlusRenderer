#pragma once

#include <vector>

#include <Engine/Common.h>
#include <Engine/Render/Context.h>
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

struct Camera;

struct ViewFrustum
{
	// Top Bottom Left Right Near Far
	Float4 Planes[6];

	void Update(const Camera& camera);

	bool IsInFrustum(const BoundingSphere& sphere) const
	{
		for (uint32_t i = 0; i < 6; i++)
		{
			const float signedDistance = Planes[i].Dot(Float4(sphere.Center.x, sphere.Center.y, sphere.Center.z, 1.0f));
			if (signedDistance < -sphere.Radius)
				return false;
			
			// Intersects plane
			if (signedDistance < sphere.Radius)
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

	struct MaterialSB
	{
		DirectX::XMFLOAT3 AlbedoFactor;
		DirectX::XMFLOAT3 FresnelR0;
		float MetallicFactor;
		float RoughnessFactor;
		uint32_t Albedo;
		uint32_t MetallicRoughness;
		uint32_t Normal;
	};
	using SBType = MaterialSB;

	operator MaterialSB () 
	{
		MaterialSB matSB{};
		matSB.AlbedoFactor = AlbedoFactor.ToXMFA();
		matSB.FresnelR0 = FresnelR0.ToXMFA();
		matSB.MetallicFactor = MetallicFactor;
		matSB.RoughnessFactor = RoughnessFactor;
		matSB.Albedo = Albedo;
		matSB.MetallicRoughness = MetallicRoughness;
		matSB.Normal = Normal;
		return matSB;
	}
};

struct Mesh
{
	uint32_t MeshIndex = 0;

	uint32_t VertCount;
	uint32_t IndexCount;

	uint32_t VertOffset;
	uint32_t IndexOffset;
	
	std::vector<DirectX::CullData> MeshletCullData;

	struct MeshSB
	{
		uint32_t VertexOffset;
		uint32_t IndexOffset;
		uint32_t IndexCount;
	};
	using SBType = MeshSB;

	operator MeshSB ()
	{
		MeshSB meshSB{};
		meshSB.VertexOffset = VertOffset;
		meshSB.IndexOffset = IndexOffset;
		meshSB.IndexCount = IndexCount;
		return meshSB;
	}
};

struct Drawable
{
	static constexpr uint32_t InvalidIndex = static_cast<uint32_t>(-1);

	uint32_t DrawableIndex = InvalidIndex;

	uint32_t MaterialIndex;
	uint32_t MeshIndex;

	Float3 Position = Float3(0.0f, 0.0f, 0.0f);
	Float3 Scale = Float3(1.0f, 1.0f, 1.0f);
	Float3 Rotation = Float3(0.0f, 0.0f, 0.0f);

	DirectX::XMFLOAT4X4 BaseTransform;
	BoundingSphere BaseBoundingVolume;

	BoundingSphere GetBoundingVolume() const;

	struct DrawableSB
	{
		// References
		uint32_t MaterialIndex;
		uint32_t MeshIndex;
		
		// Transform
		DirectX::XMFLOAT4X4 ModelToWorld;

		// BoundingVolume
		DirectX::XMFLOAT3 Center;
		float Radius;
	};
	using SBType = DrawableSB;

	operator DrawableSB ()
	{
		using namespace DirectX;

		XMMATRIX baseTransform = XMLoadFloat4x4(&BaseTransform);
		XMMATRIX modelToWorld = XMMatrixAffineTransformation(Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), Position.ToXM());
		modelToWorld = XMMatrixMultiply(baseTransform, modelToWorld);

		::BoundingSphere bs = GetBoundingVolume();

		DrawableSB drawableSB{};
		drawableSB.MaterialIndex = MaterialIndex;
		drawableSB.MeshIndex = MeshIndex;
		drawableSB.ModelToWorld = XMUtility::ToHLSLFloat4x4(modelToWorld);
		drawableSB.Center = bs.Center.ToXMF();
		drawableSB.Radius = bs.Radius;
		return drawableSB;
	}
};

struct DirectionalLight
{
	Float3 Direction;
	Float3 Radiance;
};

struct Light
{
	uint32_t LightIndex = 0;

	bool IsSpot = false;
	Float3 Position = { 0.0f, 0.0f, 0.0f };
	Float3 Radiance = { 0.0f, 0.0f, 0.0f };
	Float2 Falloff = { 0.0f, 0.0f };			// (Start, End)

	// Spot only
	Float3 Direction = { 0.0f, 0.0f, 0.0f };
	float SpotPower = 0.0f;

	struct LightSB
	{
		bool IsSpot;
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Radiance;
		DirectX::XMFLOAT2 Falloff;
		DirectX::XMFLOAT3 Direction;
		float SpotPower;
	};
	using SBType = LightSB;

	operator LightSB ()
	{
		LightSB lightSB{};
		lightSB.IsSpot = IsSpot;
		lightSB.Position = Position.ToXMF();
		lightSB.Radiance = Radiance.ToXMF();
		lightSB.Falloff = Falloff.ToXMF();
		lightSB.Direction = Direction.ToXMF();
		lightSB.SpotPower = SpotPower;
		return lightSB;
	}
};

enum class RenderGroupType : uint8_t
{
	Opaque = 0,
	AlphaDiscard = 1,
	Transparent = 2,
	Count,
};

struct RenderGroupCullingData
{
	// CPU Culling
	BitField VisibilityMask;

	// GPU Culling
	ScopedRef<Buffer> VisibilityMaskBuffer;
};

struct CameraCullingData
{
	std::unordered_map<RenderGroupType, RenderGroupCullingData> RenderGroupData;
	RenderGroupCullingData& operator[](RenderGroupType rgType) { return RenderGroupData[rgType]; }

	// Stats
	ScopedRef<Buffer> StatsBuffer;
	ScopedRef<Buffer> StatsBufferReadback;
	uint32_t TotalElements = 0;
	uint32_t VisibleElements = 0;
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
	CameraCullingData CullingData;
};

namespace ElementBufferHelp
{
	Buffer* CreateBuffer(uint32_t numElements, uint32_t stride, const std::string& debugName);
	void DeleteBuffer(Buffer* buffer);

	void BufferUpdateElement(GraphicsContext& context, Buffer* buffer, uint32_t index, const void* data);
}

// T must contain typedef SBType that represents GPU struct data
// and cast operator to SBType to convert between T to SBType
template<typename T>
class ElementBuffer
{
public:
	using SBType = T::SBType;
	static constexpr uint32_t BufferStride = sizeof(SBType);

public:
	ElementBuffer(uint32_t maxElements):
		m_MaxElements(maxElements)
	{ }

	void Initialize(const std::string& debugName = "ElementBuffer::Unnamed")
	{
		m_Storage.resize(m_MaxElements);
		if(BufferStride != 0) m_Buffer = ElementBufferHelp::CreateBuffer(m_MaxElements, BufferStride, debugName);
	}

	~ElementBuffer()
	{
		if (BufferStride != 0) ElementBufferHelp::DeleteBuffer(m_Buffer);
	}

	T& operator [] (uint32_t index) { return m_Storage[index]; }
	void MarkDirty(uint32_t index) { m_DirtyElements.insert(index); }

	// TODO: Reduce number of update element calls
	void SyncGPUBuffer(GraphicsContext& context)
	{
		if (m_DirtyElements.empty()) return;

		for (uint32_t dirtyEntry : m_DirtyElements)
		{
			const SBType sbType = (SBType) m_Storage[dirtyEntry];
			ElementBufferHelp::BufferUpdateElement(context, m_Buffer, dirtyEntry, &sbType);
		}

		m_DirtyElements.clear();
	}

	uint32_t GetSize() const { return m_NextIndex; }
	uint32_t Next() 
	{ 
		uint32_t index = m_NextIndex++;
		ASSERT(index < m_MaxElements, "index < m_MaxElements");
		return index; 
	}
	Buffer* GetBuffer() const { ASSERT(BufferStride, "m_GpuStride != 0");  return m_Buffer; }

private:
	uint32_t m_MaxElements;
	std::atomic<uint32_t> m_NextIndex;

	std::unordered_set<uint32_t> m_DirtyElements;

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
	static constexpr uint32_t REGISTER_SPACE = 1;
	static constexpr uint32_t MAX_TEXTURE_COUNT = 512;

	~TextureStorage();

	void Initialize(GraphicsContext& context);
	void Update(GraphicsContext& context);

	uint32_t AllocTexture();
	void UpdateTexture(uint32_t textureIndex, Texture* texture);
	uint32_t AddTexture(Texture* texture);

	const BindlessTable& GetBindlessTable() const { return m_Table; }

private:
	bool m_TableDirty = true;
	BindlessTable m_Table;

	ScopedRef<Texture> m_EmptyTexture;
	std::vector<Texture*> m_Textures;
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
	uint32_t AddDrawable(GraphicsContext& context, Drawable& drawable);

	void SetupPipelineInputs(GraphicsState& state);

	ElementBuffer<Material> Materials;
	ElementBuffer<Mesh> Meshes;
	ElementBuffer<Drawable> Drawables;

	TextureStorage TextureData;
	MeshStorage MeshData;
};

struct SceneGraph
{
	static constexpr uint32_t MAX_ENTITIES = 10000;
	static constexpr uint32_t MAX_LIGHTS = 100000;

	struct DirectonalLightCB
	{
		DirectX::XMFLOAT3A Direction = Float3{1.0f, 2.0f, 3.0f}.ToXMFA();
		DirectX::XMFLOAT3A Radiance = Float3{ 4.0f, 5.0f, 6.0f }.ToXMFA();
	};

	struct SceneInfoRenderData
	{
		uint32_t NumLights;
		DirectX::XMFLOAT3 Padding;

		DirectonalLightCB DirLight;
		DirectX::XMFLOAT3A AmbientRadiance = Float3{ 7.0f, 8.0f, 9.0f }.ToXMFA();

		DirectX::XMFLOAT2A ScreenSize;
		float AspectRatio;
	};

	SceneGraph();
	void InitRenderData(GraphicsContext& context);
	void FrameUpdate(GraphicsContext& context);

	Light CreatePointLight(GraphicsContext& context, Float3 position, Float3 color, Float2 falloff);
	Light CreateLight(GraphicsContext& context, Light light);

	Camera MainCamera;
	Camera ShadowCamera;

	RenderGroup RenderGroups[EnumToInt(RenderGroupType::Count)];

	ElementBuffer<Light> Lights;
	DirectionalLight DirLight;
	Float3 AmbientLight;

	SceneInfoRenderData SceneInfoData;
};

extern SceneGraph* MainSceneGraph;