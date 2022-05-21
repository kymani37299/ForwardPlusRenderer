#include "SceneGraph.h"

#include "Render/Device.h"
#include "Render/Commands.h"
#include "Render/Buffer.h"
#include "System/ApplicationConfiguration.h"

SceneGraph MainSceneGraph;

namespace
{
	struct EntitySB
	{
		DirectX::XMFLOAT4X4 ModelToWorld;
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

	struct DrawableSB
	{
		uint32_t EntityIndex;
		uint32_t MaterialIndex;
	};

	struct LightSB
	{
		uint32_t LightType;
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Strength;
		DirectX::XMFLOAT2 Falloff;
		DirectX::XMFLOAT3 Direction;
		float SpotPower;
	};

	float DegreesToRadians(float deg)
	{
		const float DEG_2_RAD = 3.1415f / 180.0f;
		return deg * DEG_2_RAD;
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

	Planes[0] = FrustumPlane{ ntr, ntl, ftl };	// Top
	Planes[1] = FrustumPlane{ nbl, nbr, fbr };	// Bottom
	Planes[2] = FrustumPlane{ ntl, nbl, fbl };	// Left
	Planes[3] = FrustumPlane{ nbr, ntr, fbr };	// Right
	Planes[4] = FrustumPlane{ ntl, ntr, nbr };	// Near
	Planes[5] = FrustumPlane{ ftr, ftl, fbl };	// Far
}

void Material::UpdateBuffer(ID3D11DeviceContext* context, RenderGroup& renderGroup)
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

void Entity::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;
	
	XMMATRIX modelToWorld = XMMatrixAffineTransformation(Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), Position.ToXM());
	EntitySB entitySB{};
	entitySB.ModelToWorld = XMUtility::ToHLSLFloat4x4(modelToWorld);
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph.Entities.GetBuffer(), sizeof(EntitySB) * EntityIndex, &entitySB, 0, sizeof(EntitySB));
}

void Drawable::UpdateBuffer(ID3D11DeviceContext* context, RenderGroup& renderGroup)
{
	using namespace DirectX;

	DrawableSB drawableSB{};
	drawableSB.EntityIndex = EntityIndex;
	drawableSB.MaterialIndex = MaterialIndex;
	GFX::Cmd::UploadToBuffer(context, renderGroup.Drawables.GetBuffer(), sizeof(DrawableSB) * DrawableIndex, &drawableSB, 0, sizeof(DrawableSB));
}

void Light::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;

	LightSB lightSB{};
	lightSB.LightType = Type;
	lightSB.Position = Position.ToXMF();
	lightSB.Strength = Strength.ToXMF();
	lightSB.Falloff = Falloff.ToXMF();
	lightSB.Direction = Direction.ToXMF();
	lightSB.SpotPower = SpotPower;
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph.Lights.GetBuffer(), sizeof(LightSB) * LightIndex, &lightSB, 0, sizeof(LightSB));
}

void Camera::RotToAxis(CameraTransform& transform)
{
	// TODO: Calculate up based on roll
	transform.Up = Float3(0.0f, 1.0f, 0.0f);
	transform.Forward = Float3((float)(std::cos(transform.Rotation.y) * std::cos(transform.Rotation.x)), (float)(std::sin(transform.Rotation.x)), (float)(std::sin(transform.Rotation.y) * std::cos(transform.Rotation.x))).Normalize();
	transform.Right = transform.Forward.Cross(transform.Up).Normalize();
	transform.Up = transform.Right.Cross(transform.Forward).Normalize();
}

Camera::Camera(Float3 position, Float3 rotation, float fov):
	CurrentTranform({ position, rotation }),
	LastTransform({ position, rotation }),
	NextTransform({ position, rotation }),
	FOV(fov),
	ZFar(1000.0f),
	ZNear(0.1f)
{ }

void Camera::UpdateBufferForTransform(ID3D11DeviceContext* context, CameraTransform& transform, BufferID& destBuffer)
{
	using namespace DirectX;
	struct CameraCB
	{
		XMFLOAT4X4 WorldToView;
		XMFLOAT4X4 ViewToClip;
		XMFLOAT4X4 ClipToWorld;
		XMFLOAT3A Position;
		XMFLOAT2A Jitter;
	};

	AspectRatio = (float) AppConfig.WindowWidth / AppConfig.WindowHeight;
	RotToAxis(transform);

	XMMATRIX worldToView = XMMatrixLookAtLH(transform.Position.ToXM(), (transform.Position + transform.Forward).ToXM(), transform.Up.ToXM());
	XMMATRIX viewToClip = XMMatrixPerspectiveFovLH(DegreesToRadians(FOV), AspectRatio, ZNear, ZFar);
	XMMATRIX worldToClip = XMMatrixMultiply(worldToView, viewToClip);
	XMMATRIX clipToWorld = XMMatrixInverse(nullptr, worldToClip);

	WorldToView = XMMatrixTranspose(worldToView);

	CameraCB cameraCB{};
	cameraCB.WorldToView = XMUtility::ToHLSLFloat4x4(worldToView);
	cameraCB.ViewToClip = XMUtility::ToHLSLFloat4x4(viewToClip);
	cameraCB.ClipToWorld = XMUtility::ToHLSLFloat4x4(clipToWorld);
	cameraCB.Position = transform.Position.ToXMFA();
	cameraCB.Jitter = UseJitter ? Jitter[JitterIndex].ToXMFA() : Float2(0.0f, 0.0f).ToXMFA();

	if (!destBuffer.Valid()) destBuffer = GFX::CreateConstantBuffer<CameraCB>();
	GFX::Cmd::UploadToBuffer(context, destBuffer, 0, &cameraCB, 0, sizeof(CameraCB));
}

void Camera::UpdateBuffers(ID3D11DeviceContext* context)
{
	UpdateBufferForTransform(context, CurrentTranform, CameraBuffer);
	UpdateBufferForTransform(context, LastTransform, LastFrameCameraBuffer);
}

void Camera::FrameUpdate(ID3D11DeviceContext* context)
{
	LastTransform = CurrentTranform;
	CurrentTranform = NextTransform;
	JitterIndex = (JitterIndex + 1) % 16;
	UpdateBuffers(context);
	CameraFrustum.Update(*this);
}

RenderGroup::RenderGroup():
	Materials(MAX_DRAWABLES, sizeof(MaterialSB)),
	Meshes(MAX_DRAWABLES, 0),
	Drawables(MAX_DRAWABLES, sizeof(DrawableSB))
{

}

void RenderGroup::Initialize(ID3D11DeviceContext* context)
{

	Materials.Initialize();
	Meshes.Initialize();
	Drawables.Initialize();
	MeshData.Initialize();
	TextureData = GFX::CreateTextureArray(TEXTURE_SIZE, TEXTURE_SIZE, MAX_TEXTURES, RCF_Bind_SRV | RCF_CopyDest, TEXTURE_MIPS);
}

Drawable RenderGroup::CreateDrawable(ID3D11DeviceContext* context, Material& material, Mesh& mesh, BoundingSphere& boundingSphere, const Entity& entity)
{
	const uint32_t dIndex = Drawables.Next();
	const uint32_t matIndex = Materials.Next();
	const uint32_t mIndex = Meshes.Next();

	Drawable drawable;
	drawable.DrawableIndex = dIndex;
	drawable.MaterialIndex = matIndex;
	drawable.MeshIndex = mIndex;
	drawable.EntityIndex = entity.EntityIndex;
	drawable.BoundingVolume = boundingSphere;

	material.MaterialIndex = matIndex;
	mesh.MeshIndex = mIndex;

	Drawables[dIndex] = drawable;
	Materials[matIndex] = material;
	Meshes[mIndex] = mesh;

	if (Device::Get()->IsMainContext(context))
	{
		material.UpdateBuffer(context, *this);
		drawable.UpdateBuffer(context, *this);
	}
	else
	{
		NOT_IMPLEMENTED;
	}

	return drawable;
}

SceneGraph::SceneGraph() :
	Entities(MAX_ENTITIES, sizeof(EntitySB)),
	Lights(MAX_LIGHTS, sizeof(LightSB))
{

}

void SceneGraph::InitRenderData(ID3D11DeviceContext* context)
{
	for (uint32_t i = 0; i < EnumToInt(RenderGroupType::Count); i++)
	{
		RenderGroups[i].Initialize(context);
	}
	Entities.Initialize();
	Lights.Initialize();
}

void SceneGraph::FrameUpdate(ID3D11DeviceContext* context)
{
	MainCamera.FrameUpdate(context);

	// Scene info
	{
		struct SceneInfoCB
		{
			uint32_t NumLights;
			DirectX::XMFLOAT3 Padding;
			DirectX::XMFLOAT2A ScreenSize;
			float AspectRatio;
		};

		SceneInfoCB sceneInfoCB{};
		sceneInfoCB.NumLights = Lights.GetSize();
		sceneInfoCB.ScreenSize = { (float) AppConfig.WindowWidth, (float) AppConfig.WindowHeight };
		sceneInfoCB.AspectRatio = (float) AppConfig.WindowWidth / AppConfig.WindowHeight;

		if (!SceneInfoBuffer.Valid()) SceneInfoBuffer = GFX::CreateConstantBuffer<SceneInfoCB>();

		GFX::Cmd::UploadToBuffer(context, SceneInfoBuffer, 0, &sceneInfoCB, 0, sizeof(SceneInfoCB));
	}
}

Entity& SceneGraph::CreateEntity(ID3D11DeviceContext* context, Float3 position, Float3 scale)
{
	const uint32_t eIndex = Entities.Next();

	Entity& e = Entities[eIndex];
	e.EntityIndex = eIndex;
	e.Position = position;
	e.Scale = scale;
	e.UpdateBuffer(context);

	return Entities[eIndex];
}

Light SceneGraph::CreateDirectionalLight(ID3D11DeviceContext* context, Float3 direction, Float3 color)
{
	Light l{};
	l.Type = LT_Directional;
	l.Direction = direction;
	l.Strength = color;

	return CreateLight(context, l);
}

Light SceneGraph::CreateAmbientLight(ID3D11DeviceContext* context, Float3 color)
{
	Light l{};
	l.Type = LT_Ambient;
	l.Strength = color;
	return CreateLight(context, l);
}

Light SceneGraph::CreatePointLight(ID3D11DeviceContext* context, Float3 position, Float3 color, Float2 falloff)
{
	Light l{};
	l.Type = LT_Point;
	l.Position = position;
	l.Strength = color;
	l.Falloff = falloff;
	return CreateLight(context, l);
}

Light SceneGraph::CreateLight(ID3D11DeviceContext* context, Light light)
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
	BufferID CreateBuffer(uint32_t numElements, uint32_t stride) { return GFX::CreateBuffer(numElements * stride, stride, RCF_Bind_SB | RCF_CPU_Write_Persistent); }
	void DeleteBuffer(BufferID buffer) { GFX::Storage::Free(buffer); }
}

void MeshStorage::Initialize()
{
	m_PositionBuffer = GFX::CreateBuffer(GetPositionStride(), GetPositionStride(), RCF_Bind_VB | RCF_CopyDest);
	m_TexcoordBuffer = GFX::CreateBuffer(GetTexroordStride(), GetTexroordStride(), RCF_Bind_VB | RCF_CopyDest);
	m_NormalBuffer = GFX::CreateBuffer(GetNormalStride(), GetNormalStride(), RCF_Bind_VB | RCF_CopyDest);
	m_TangentBuffer = GFX::CreateBuffer(GetTangentStride(), GetTangentStride(), RCF_Bind_VB | RCF_CopyDest);
	m_DrawableIndexBuffer = GFX::CreateBuffer(GetDrawableIndexStride(), GetDrawableIndexStride(), RCF_Bind_VB | RCF_CopyDest);
}

MeshStorage::Allocation MeshStorage::Allocate(ID3D11DeviceContext* context, uint32_t vertexCount, uint32_t indexCount)
{
	MeshStorage::Allocation alloc{};
	alloc.VertexOffset = m_VertexCount.fetch_add(vertexCount);
	alloc.IndexOffset = m_IndexCount.fetch_add(indexCount);

	const uint32_t wantedVBSize = vertexCount + alloc.VertexOffset;
	const uint32_t wantedIBSize = indexCount + alloc.IndexOffset;
	GFX::ExpandBuffer(context, m_PositionBuffer, wantedVBSize * GetPositionStride());
	GFX::ExpandBuffer(context, m_TexcoordBuffer, wantedVBSize * GetTexroordStride());
	GFX::ExpandBuffer(context, m_NormalBuffer, wantedVBSize * GetNormalStride());
	GFX::ExpandBuffer(context, m_TangentBuffer, wantedVBSize * GetTangentStride());
	GFX::ExpandBuffer(context, m_DrawableIndexBuffer, wantedVBSize * GetDrawableIndexStride());
	m_IndexBuffer.resize((size_t) wantedIBSize * GetIndexBufferStride());

	return alloc;
}