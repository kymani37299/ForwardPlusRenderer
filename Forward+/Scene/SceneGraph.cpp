#include "SceneGraph.h"

#include <Engine/Render/Device.h>
#include <Engine/Render/Commands.h>
#include <Engine/Render/Context.h>
#include <Engine/Render/RenderThread.h>
#include <Engine/Render/Buffer.h>
#include <Engine/Render/Texture.h>
#include <Engine/Render/Shader.h>
#include <Engine/System/ApplicationConfiguration.h>

#include "Renderers/Util/SamplerManager.h"

SceneGraph* MainSceneGraph = nullptr;

namespace
{
	struct EntitySB
	{
		DirectX::XMFLOAT4X4 ModelToWorld;

		// BoundingVolume
		DirectX::XMFLOAT3 Center;
		float Radius;
	};

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
	
	struct MeshSB
	{
		uint32_t VertexOffset;
		uint32_t IndexOffset;
		uint32_t IndexCount;
	};

	struct DrawableSB
	{
		uint32_t EntityIndex;
		uint32_t MaterialIndex;
		uint32_t MeshIndex;
	};

	struct LightSB
	{
		bool IsSpot;
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Radiance;
		DirectX::XMFLOAT2 Falloff;
		DirectX::XMFLOAT3 Direction;
		float SpotPower;
	};

	float DegreesToRadians(float deg)
	{
		const float DEG_2_RAD = 3.1415f / 180.0f;
		return deg * DEG_2_RAD;
	}

	Float4 CreateFrustumPlane(Float3 p0, Float3 p1, Float3 p2)
	{
		const Float3 v = p1 - p0;
		const Float3 u = p2 - p0;
		const Float3 normal = v.Cross(u).Normalize();

		return Float4(normal.x, normal.y, normal.z, -normal.Dot(p0));
	}
}

void ViewFrustum::Update(const Camera& c)
{
	const Camera::CameraTransform& t = c.CurrentTranform;

	// Near and far plane dimensions
	const float fovTan = tanf(DegreesToRadians(c.FOV / 2.0f));
	const float hnear = 2.0f * fovTan * c.ZNear;
	const float wnear = hnear * c.AspectRatio;
	const float hfar = 2.0f * fovTan * c.ZFar;
	const float wfar = hfar * c.AspectRatio;

	// 8 points of the frustum
	const Float3 fc = t.Position + c.ZFar * t.Forward;
	const Float3 nc = t.Position + c.ZNear * t.Forward;

	const Float3 ftl = fc + (hfar / 2.0f) * t.Up - (wfar / 2.0f) * t.Right;
	const Float3 ftr = fc + (hfar / 2.0f) * t.Up + (wfar / 2.0f) * t.Right;
	const Float3 fbl = fc - (hfar / 2.0f) * t.Up - (wfar / 2.0f) * t.Right;
	const Float3 fbr = fc - (hfar / 2.0f) * t.Up + (wfar / 2.0f) * t.Right;

	const Float3 ntl = nc + (hnear / 2.0f) * t.Up - (wnear / 2.0f) * t.Right;
	const Float3 ntr = nc + (hnear / 2.0f) * t.Up + (wnear / 2.0f) * t.Right;
	const Float3 nbl = nc - (hnear / 2.0f) * t.Up - (wnear / 2.0f) * t.Right;
	const Float3 nbr = nc - (hnear / 2.0f) * t.Up + (wnear / 2.0f) * t.Right;

	Planes[0] = CreateFrustumPlane( ntr, ntl, ftl );	// Top
	Planes[1] = CreateFrustumPlane( nbl, nbr, fbr );	// Bottom
	Planes[2] = CreateFrustumPlane( ntl, nbl, fbl );	// Left
	Planes[3] = CreateFrustumPlane( nbr, ntr, fbr );	// Right
	Planes[4] = CreateFrustumPlane( ntl, ntr, nbr );	// Near
	Planes[5] = CreateFrustumPlane(ftr, ftl, fbl);	// Far
}

void Material::UpdateBuffer(GraphicsContext& context, RenderGroup& renderGroup)
{
	using namespace DirectX;
	
	Material& mat = renderGroup.Materials[MaterialIndex];

	MaterialSB matSB{};
	matSB.AlbedoFactor = mat.AlbedoFactor.ToXMFA();
	matSB.FresnelR0 = mat.FresnelR0.ToXMFA();
	matSB.MetallicFactor = mat.MetallicFactor;
	matSB.RoughnessFactor = mat.RoughnessFactor;
	matSB.Albedo = mat.Albedo;
	matSB.MetallicRoughness = mat.MetallicRoughness;
	matSB.Normal = mat.Normal;
	GFX::Cmd::UploadToBuffer(context, renderGroup.Materials.GetBuffer(), MaterialIndex * sizeof(MaterialSB), &matSB, 0, sizeof(MaterialSB));
}

void Mesh::UpdateBuffer(GraphicsContext& context, RenderGroup& renderGroup)
{
	using namespace DirectX;

	MeshSB meshSB{};
	meshSB.VertexOffset = VertOffset;
	meshSB.IndexOffset = IndexOffset;
	meshSB.IndexCount = IndexCount;
	GFX::Cmd::UploadToBuffer(context, renderGroup.Meshes.GetBuffer(), sizeof(MeshSB) * MeshIndex, &meshSB, 0, sizeof(MeshSB));
}

BoundingSphere Entity::GetBoundingVolume() const
{
	const BoundingSphere& bv = BaseBoundingSphere;

	const Float3 baseScale{ BaseTransform(0, 0), BaseTransform(1, 1), BaseTransform(2, 2) };
	const float maxBaseScale = MAX(MAX(baseScale.x, baseScale.y), baseScale.z);
	const float maxlocalScale = MAX(MAX(Scale.x, Scale.y), Scale.z);
	const float maxScale = maxBaseScale * maxlocalScale;

	const DirectX::XMMATRIX baseTransform = DirectX::XMLoadFloat4x4(&BaseTransform);
	const DirectX::XMVECTOR xmbasePosition = DirectX::XMVector4Transform(Float4{ bv.Center.x, bv.Center.y, bv.Center.z, 1.0f }, baseTransform);
	const Float3 basePosition{ xmbasePosition };

	BoundingSphere bs;
	bs.Center = Position + baseScale * Scale * basePosition;
	bs.Radius = bv.Radius * maxScale;

	return bs;
}

void Entity::UpdateBuffer(GraphicsContext& context)
{
	using namespace DirectX;
	
	XMMATRIX baseTransform = XMLoadFloat4x4(&BaseTransform);
	XMMATRIX modelToWorld = XMMatrixAffineTransformation(Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), Position.ToXM());
	modelToWorld = XMMatrixMultiply(baseTransform, modelToWorld);

	::BoundingSphere bs = GetBoundingVolume();

	EntitySB entitySB{};
	entitySB.ModelToWorld = XMUtility::ToHLSLFloat4x4(modelToWorld);
	entitySB.Center = bs.Center.ToXMF();
	entitySB.Radius = bs.Radius;
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph->Entities.GetBuffer(), sizeof(EntitySB) * EntityIndex, &entitySB, 0, sizeof(EntitySB));
}

void Drawable::UpdateBuffer(GraphicsContext& context, RenderGroup& renderGroup)
{
	using namespace DirectX;

	DrawableSB drawableSB{};
	drawableSB.EntityIndex = EntityIndex;
	drawableSB.MaterialIndex = MaterialIndex;
	drawableSB.MeshIndex = MeshIndex;
	GFX::Cmd::UploadToBuffer(context, renderGroup.Drawables.GetBuffer(), sizeof(DrawableSB) * DrawableIndex, &drawableSB, 0, sizeof(DrawableSB));
}

void Light::UpdateBuffer(GraphicsContext& context)
{
	using namespace DirectX;

	LightSB lightSB{};
	lightSB.IsSpot = IsSpot;
	lightSB.Position = Position.ToXMF();
	lightSB.Radiance = Radiance.ToXMF();
	lightSB.Falloff = Falloff.ToXMF();
	lightSB.Direction = Direction.ToXMF();
	lightSB.SpotPower = SpotPower;
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph->Lights.GetBuffer(), sizeof(LightSB) * LightIndex, &lightSB, 0, sizeof(LightSB));
}

Camera Camera::CreatePerspective(float fov, float aspect, float znear, float zfar)
{
	Camera cam;
	cam.Type = CameraType::Perspective;
	cam.AspectRatio = aspect;
	cam.FOV = fov;
	cam.ZNear = znear;
	cam.ZFar = zfar;
	return cam;
}

Camera Camera::CreateOrtho(float rectWidth, float rectHeight, float znear, float zfar)
{
	Camera cam;
	cam.Type = CameraType::Ortho;
	cam.RectWidth = rectWidth;
	cam.RectHeight = rectHeight;
	cam.ZNear = znear;
	cam.ZFar = zfar;
	return cam;
}

Float3 CalcForwardVector(float pitch, float yaw)
{
	const float x = std::cosf(yaw) * std::cosf(pitch);
	const float y = std::sinf(pitch);
	const float z = std::sinf(yaw) * std::cos(pitch);
	return Float3{ x,y,z }.Normalize();
}

void Camera::UpdateRenderDataForTransform(CameraTransform& transform, CameraRenderData& destData)
{
	using namespace DirectX;

	if (UseRotation)
	{
		transform.Forward = CalcForwardVector(transform.Rotation.x, transform.Rotation.y);
	}
	
	// TODO: Calc up based on roll
	transform.Up = Float3(0.0f, 1.0f, 0.0f);
	transform.Right = transform.Forward.Cross(transform.Up).Normalize();
	transform.Up = transform.Right.Cross(transform.Forward).Normalize();

	XMMATRIX worldToView = XMMatrixLookAtLH(transform.Position.ToXM(), (transform.Position + transform.Forward).ToXM(), transform.Up.ToXM());
	XMMATRIX viewToClip;

	if (Type == CameraType::Perspective) viewToClip = XMMatrixPerspectiveFovLH(DegreesToRadians(FOV), AspectRatio, ZNear, ZFar);
	else if (Type == CameraType::Ortho) viewToClip = XMMatrixOrthographicLH(RectWidth, RectHeight, ZNear, ZFar);

	XMMATRIX worldToClip = XMMatrixMultiply(worldToView, viewToClip);
	XMMATRIX clipToWorld = XMMatrixInverse(nullptr, worldToClip);

	WorldToView = XMMatrixTranspose(worldToView);

	destData.WorldToView = XMUtility::ToHLSLFloat4x4(worldToView);
	destData.ViewToClip = XMUtility::ToHLSLFloat4x4(viewToClip);
	destData.ClipToWorld = XMUtility::ToHLSLFloat4x4(clipToWorld);
	destData.Position = transform.Position.ToXMFA();
	destData.Jitter = UseJitter ? Jitter[JitterIndex].ToXMFA() : Float2(0.0f, 0.0f).ToXMFA();
}

void Camera::UpdateRenderData()
{
	UpdateRenderDataForTransform(CurrentTranform, CameraData);
	UpdateRenderDataForTransform(LastTransform, LastCameraData);
}

void Camera::FrameUpdate(GraphicsContext& context)
{
	LastTransform = CurrentTranform;
	CurrentTranform = NextTransform;
	JitterIndex = (JitterIndex + 1) % 16;
	UpdateRenderData();
	CameraFrustum.Update(*this);
}

RenderGroup::RenderGroup():
	Materials(MAX_DRAWABLES, sizeof(MaterialSB)),
	Meshes(MAX_DRAWABLES, sizeof(MeshSB)),
	Drawables(MAX_DRAWABLES, sizeof(DrawableSB))
{

}

void RenderGroup::Initialize(GraphicsContext& context)
{
	Materials.Initialize("ElementBuffer::Materials");
	Meshes.Initialize("ElementBuffer::Meshes");
	Drawables.Initialize("ElementBuffer::Drawables");
	MeshData.Initialize();
	TextureData.Initialize(context);

	VisibilityMaskBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_UAV));
}

uint32_t RenderGroup::AddMaterial(GraphicsContext& context, Material& material)
{
	const uint32_t index = Materials.Next();
	material.MaterialIndex = index;
	Materials[index] = material;

	if (Device::Get()->IsMainContext(context))
	{
		material.UpdateBuffer(context, *this);
	}
	else
	{
		NOT_IMPLEMENTED;
	}
	return index;
}

uint32_t RenderGroup::AddMesh(GraphicsContext& context, Mesh& mesh)
{
	const uint32_t index = (uint32_t) Meshes.Next();
	mesh.MeshIndex = index;
	Meshes[index] = mesh;

	if (Device::Get()->IsMainContext(context))
	{
		mesh.UpdateBuffer(context, *this);
	}
	else
	{
		NOT_IMPLEMENTED;
	}

	return index;
}

class RenderGroupAddDrawRenderTask : public RenderTask
{
public:
	RenderGroupAddDrawRenderTask(Drawable& drawable, RenderGroup& renderGroup):
		m_RenderGroup(renderGroup),
		m_Drawable(drawable) {}

	void Run(GraphicsContext& context) override
	{
		Entity& e = MainSceneGraph->Entities[m_Drawable.EntityIndex];

		e.UpdateBuffer(context);
		m_Drawable.UpdateBuffer(context, m_RenderGroup);
	}

private:
	RenderGroup& m_RenderGroup;
	Drawable& m_Drawable;
};

void RenderGroup::AddDraw(GraphicsContext& context, uint32_t materialIndex, uint32_t meshIndex, uint32_t entityIndex, const BoundingSphere& boundingSphere)
{
	const uint32_t index = Drawables.Next();
	
	Entity& e = MainSceneGraph->Entities[entityIndex];

	Drawable drawable;
	drawable.DrawableIndex = index;
	drawable.MaterialIndex = materialIndex;
	drawable.MeshIndex = meshIndex;
	drawable.EntityIndex = entityIndex;
	Drawables[index] = drawable;

	e.BaseBoundingSphere = boundingSphere;

	if (Device::Get()->IsMainContext(context))
	{
		Device::Get()->GetTaskExecutor().Submit(new RenderGroupAddDrawRenderTask(Drawables[index], *this));
	}
	else
	{
		RenderThreadPool::Get()->Submit(new RenderGroupAddDrawRenderTask(Drawables[index], *this));
	}
}

void RenderGroup::SetupPipelineInputs(GraphicsState& state)
{
	if (state.Table.SRVs.size() < 127) state.Table.SRVs.resize(127);
	if (state.BindlessTables.empty()) state.BindlessTables.resize(1);

	state.Table.SRVs[124] = MainSceneGraph->Entities.GetBuffer();
	state.Table.SRVs[125] = Materials.GetBuffer();
	state.Table.SRVs[126] = Drawables.GetBuffer();
	state.BindlessTables[0] = TextureData.GetBindlessTable();
}

SceneGraph::SceneGraph() :
	Entities(MAX_ENTITIES, sizeof(EntitySB)),
	Lights(MAX_LIGHTS, sizeof(LightSB))
{

}

void SceneGraph::InitRenderData(GraphicsContext& context)
{
	MainCamera = Camera::CreatePerspective(75.0f, (float) AppConfig.WindowWidth / AppConfig.WindowHeight, 0.1f, 1000.0f);
	ShadowCamera = Camera::CreateOrtho(500.0f, 500.0f, -500.0f, 500.0f);
	ShadowCamera.UseRotation = false;
	
	for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
	{
		RenderGroups[i].Initialize(context);
	}
	Entities.Initialize("ElementBuffer::Entities");
	Lights.Initialize("ElementBuffer::Lights");
}

void SceneGraph::FrameUpdate(GraphicsContext& context)
{
	MainCamera.FrameUpdate(context);

	// Render group date
	for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
	{
		RenderGroups[i].TextureData.Update(context);
	}

	// Shadow camera
	{
		ShadowCamera.NextTransform.Position = MainCamera.CurrentTranform.Position;
		ShadowCamera.NextTransform.Forward = DirLight.Direction;
		ShadowCamera.FrameUpdate(context);
	}

	// Scene info
	{
		SceneInfoData.NumLights = Lights.GetSize();
		SceneInfoData.ScreenSize = { (float) AppConfig.WindowWidth, (float) AppConfig.WindowHeight };
		SceneInfoData.AspectRatio = (float) AppConfig.WindowWidth / AppConfig.WindowHeight;

		SceneInfoData.DirLight.Direction = DirLight.Direction.ToXMFA();
		SceneInfoData.DirLight.Radiance = DirLight.Radiance.ToXMFA();
		SceneInfoData.AmbientRadiance = AmbientLight.ToXMFA();
	}
}

uint32_t SceneGraph::AddEntity(GraphicsContext& context, Entity entity)
{
	const uint32_t index = (uint32_t) Entities.Next();
	entity.EntityIndex = index;
	Entities[index] = entity;
	
	if (Device::Get()->IsMainContext(context))
	{
		entity.UpdateBuffer(context);
	}
	else
	{
		NOT_IMPLEMENTED;
	}

	return index;
}

Light SceneGraph::CreatePointLight(GraphicsContext& context, Float3 position, Float3 color, Float2 falloff)
{
	Light l{};
	l.IsSpot = false;
	l.Position = position;
	l.Radiance = color;
	l.Falloff = falloff;
	return CreateLight(context, l);
}

Light SceneGraph::CreateLight(GraphicsContext& context, Light light)
{
	const uint32_t lightIndex = Lights.Next();
	light.LightIndex = lightIndex;

	Lights[lightIndex] = light;

	if (Device::Get()->IsMainContext(context))
	{
		light.UpdateBuffer(context);
	}
	else
	{
		NOT_IMPLEMENTED;
	}
	return light;
}

namespace ElementBufferHelp
{
	Buffer* CreateBuffer(uint32_t numElements, uint32_t stride, const std::string& debugName) 
	{
		Buffer* buffer = GFX::CreateBuffer(numElements * stride, stride, RCF_None);
		GFX::SetDebugName(buffer, debugName);
		return buffer;
	}

	void DeleteBuffer(Buffer* buffer) { delete buffer; }
}

MeshStorage::MeshStorage()
{
}

MeshStorage::~MeshStorage()
{
}

void MeshStorage::Initialize()
{
	m_VertexBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(GetVertexBufferStride(), GetVertexBufferStride(), RCF_None));
	m_IndexBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(GetIndexBufferStride(), GetIndexBufferStride(), RCF_None));

	GFX::SetDebugName(m_VertexBuffer.get(), "MeshStorage::VertexBuffer");
	GFX::SetDebugName(m_IndexBuffer.get(), "MeshStorage::IndexBuffer");
}

MeshStorage::Allocation MeshStorage::Allocate(GraphicsContext& context, uint32_t vertexCount, uint32_t indexCount)
{
	MeshStorage::Allocation alloc{};
	alloc.VertexOffset = m_VertexCount.fetch_add(vertexCount);
	alloc.IndexOffset = m_IndexCount.fetch_add(indexCount);

	const uint32_t wantedVBSize = vertexCount + alloc.VertexOffset;
	const uint32_t wantedIBSize = indexCount + alloc.IndexOffset;

	GFX::ExpandBuffer(context, m_VertexBuffer.get(), wantedVBSize * GetVertexBufferStride());
	GFX::ExpandBuffer(context, m_IndexBuffer.get(), wantedIBSize * GetIndexBufferStride());

	return alloc;
}

TextureStorage::~TextureStorage()
{
	for (Texture* tex : m_Textures) 
		if(tex != m_EmptyTexture.get()) 
			delete tex;
}

void TextureStorage::Initialize(GraphicsContext& context)
{
	ColorUNORM defaultColor{ (unsigned char) 0, 0, 0, 255 };
	ResourceInitData initData = { &context, &defaultColor };
	m_EmptyTexture = ScopedRef<Texture>(GFX::CreateTexture(1, 1, RCF_None, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &initData));
	m_Textures.resize(MAX_TEXTURE_COUNT);

	for(uint32_t i=0;i<MAX_TEXTURE_COUNT;i++)
		m_Textures[i] = m_EmptyTexture.get();
}

void TextureStorage::Update(GraphicsContext& context)
{
	while (m_TableDirty)
	{
		m_TableDirty = false;
		GFX::Cmd::ReleaseBindlessTable(context, m_Table);
		m_Table = GFX::Cmd::CreateBindlessTable(context, m_Textures, REGISTER_SPACE);
	}
}

uint32_t TextureStorage::AllocTexture()
{
	const uint32_t textureIndex = m_NextAllocation++;
	ASSERT(m_NextAllocation < MAX_TEXTURE_COUNT, "[TextureStorage] Too many textures!");
	m_Textures[textureIndex] = m_EmptyTexture.get();
	m_TableDirty = true;
	return textureIndex;
}

void TextureStorage::UpdateTexture(uint32_t textureIndex, Texture* texture)
{
	m_Textures[textureIndex] = texture;
	m_TableDirty = true;
}

uint32_t TextureStorage::AddTexture(Texture* texture)
{
	const uint32_t texIndex = AllocTexture();
	UpdateTexture(texIndex, texture);
	return texIndex;

}
