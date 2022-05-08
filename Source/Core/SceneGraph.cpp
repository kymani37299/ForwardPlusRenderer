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

void Material::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;
	
	Material& mat = MainSceneGraph.Materials[MaterialIndex];

	MaterialSB matSB{};
	matSB.AlbedoFactor = mat.AlbedoFactor.ToXMFA();
	matSB.FresnelR0 = mat.FresnelR0.ToXMFA();
	matSB.MetallicFactor = mat.MetallicFactor;
	matSB.RoughnessFactor = mat.RoughnessFactor;
	matSB.Albedo = mat.Albedo;
	matSB.MetallicRoughness = mat.MetallicRoughness;
	matSB.Normal = mat.Normal;
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph.Materials.GetBuffer(), MaterialIndex * sizeof(MaterialSB), &matSB, 0, sizeof(MaterialSB));
}

void Entity::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;
	
	XMMATRIX modelToWorld = XMMatrixTranspose(XMMatrixAffineTransformation(Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), Position.ToXM()));
	EntitySB entitySB{};
	entitySB.ModelToWorld = XMUtility::ToXMFloat4x4(modelToWorld);
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph.Entities.GetBuffer(), sizeof(EntitySB) * EntityIndex, &entitySB, 0, sizeof(EntitySB));
}

void Drawable::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;

	DrawableSB drawableSB{};
	drawableSB.EntityIndex = EntityIndex;
	drawableSB.MaterialIndex = MaterialIndex;
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph.Drawables.GetBuffer(), sizeof(DrawableSB) * DrawableIndex, &drawableSB, 0, sizeof(DrawableSB));
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

void Camera::RotToAxis(Float3 rot, Float3& forward, Float3& up, Float3& right)
{
	using namespace DirectX;

	// TODO: Calculate up based on roll
	up = Float3(0.0f, 1.0f, 0.0f);
	forward = Float3((float)(std::cos(rot.y) * std::cos(rot.x)), (float)(std::sin(rot.x)), (float)(std::sin(rot.y) * std::cos(rot.x)));

	XMVECTOR vec = XMVector3Cross(forward.ToXM(), up.ToXM());
	vec = XMVector3Normalize(vec);
	right = Float3(vec);
}

Camera::Camera(Float3 position, Float3 rotation, float fov):
	Position(position),
	Rotation(rotation),
	FOV(fov),
	ZFar(1000.0f),
	ZNear(0.1f)
{ }

void Camera::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;
	struct CameraCB
	{
		XMFLOAT4X4 WorldToView;
		XMFLOAT4X4 ViewToClip;
		XMFLOAT3 Position;
	};

	AspectRatio = (float) AppConfig.WindowWidth / AppConfig.WindowHeight;
	RotToAxis(Rotation, Forward, Up, Right);

	XMMATRIX matView = XMMatrixTranspose(XMMatrixLookAtLH(Position.ToXM(), (Position + Forward).ToXM(), Up.ToXM()));
	XMMATRIX matProj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(DegreesToRadians(FOV), AspectRatio, ZNear, ZFar));
	
	WorldToView = matView;

	CameraCB cameraCB{};
	cameraCB.WorldToView = XMUtility::ToXMFloat4x4(matView);
	cameraCB.ViewToClip = XMUtility::ToXMFloat4x4(matProj);
	cameraCB.Position = Position.ToXMF();

	if (!CameraBuffer.Valid()) CameraBuffer = GFX::CreateConstantBuffer<CameraCB>();
	GFX::Cmd::UploadToBuffer(context, CameraBuffer, 0, &cameraCB, 0, sizeof(CameraCB));
}

SceneGraph::SceneGraph() :
	Entities(MAX_ENTITIES, sizeof(EntitySB)),
	Materials(MAX_DRAWABLES, sizeof(MaterialSB)),
	Meshes(MAX_DRAWABLES, 0),
	Drawables(MAX_DRAWABLES, sizeof(DrawableSB)),
	Lights(MAX_LIGHTS, sizeof(LightSB))
{

}

void SceneGraph::InitRenderData(ID3D11DeviceContext* context)
{
	Entities.Initialize();
	Materials.Initialize();
	Meshes.Initialize();
	Drawables.Initialize();
	Lights.Initialize();

	Geometries.Initialize();

	Textures = GFX::CreateTextureArray(TEXTURE_SIZE, TEXTURE_SIZE, MAX_TEXTURES, RCF_Bind_SRV | RCF_CopyDest, TEXTURE_MIPS);
}

void SceneGraph::FrameUpdate(ID3D11DeviceContext* context)
{
	// Camera
	{
		MainCamera.UpdateBuffer(context);

		ViewFrustum& vf = MainCamera.CameraFrustum;
		const Camera& c = MainCamera;

		const float halfVSide = c.ZFar * tanf(c.FOV * .5f);
		const float halfHSide = halfVSide * c.AspectRatio;
		const Float3 frontMultFar = c.ZFar * c.Forward;

		vf.Near = { c.Position + c.ZNear * c.Forward, c.Forward };
		vf.Far = { c.Position + frontMultFar, -1.0f * c.Forward };
		vf.Right = { c.Position, DirectX::XMVector3Cross(c.Up, frontMultFar + halfHSide * c.Right) };
		vf.Left = { c.Position, DirectX::XMVector3Cross(frontMultFar - halfHSide * c.Right, c.Up) };
		vf.Top = { c.Position, DirectX::XMVector3Cross(c.Right, frontMultFar - halfVSide * c.Up) };
		vf.Bottom = { c.Position, DirectX::XMVector3Cross(frontMultFar + halfVSide * c.Up, c.Right) };
	}

	// Scene info
	{
		struct SceneInfoCB
		{
			uint32_t NumLights;
		};

		SceneInfoCB sceneInfoCB{};
		sceneInfoCB.NumLights = Lights.GetSize();

		if (!SceneInfoBuffer.Valid()) SceneInfoBuffer = GFX::CreateConstantBuffer<SceneInfoCB>();

		GFX::Cmd::UploadToBuffer(context, SceneInfoBuffer, 0, &sceneInfoCB, 0, sizeof(SceneInfoCB));
	}

	// Peding actions
	{
		auto f = [&context, this](Drawable d)
		{
			Materials[d.MaterialIndex].UpdateBuffer(context);
			d.UpdateBuffer(context);
		};
		DrawablesToUpdate.ForEachAndClear(f);
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

Drawable SceneGraph::CreateDrawable(ID3D11DeviceContext* context, Material& material, Mesh& mesh, BoundingSphere& boundingSphere, const Entity& entity)
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
		material.UpdateBuffer(context);
		drawable.UpdateBuffer(context);
	}
	else
	{
		MainSceneGraph.DrawablesToUpdate.Add(drawable);
	}
	
	return drawable;
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