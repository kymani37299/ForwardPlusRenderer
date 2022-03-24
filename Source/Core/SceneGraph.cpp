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
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph.Entities.GetBuffer(), sizeof(EntitySB) * Index, &entitySB, 0, sizeof(EntitySB));
}

void Drawable::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;

	DrawableSB drawableSB{};
	drawableSB.EntityIndex = EntityIndex;
	drawableSB.MaterialIndex = MaterialIndex;
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph.Drawables.GetBuffer(), sizeof(DrawableSB) * DrawableIndex, &drawableSB, 0, sizeof(DrawableSB));
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
	FOV(fov)
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

	float aspectRatio = (float) AppConfig.WindowWidth / AppConfig.WindowHeight;
	Float3 Up;
	Float3 Forward;
	Float3 Right;
	RotToAxis(Rotation, Forward, Up, Right);

	XMMATRIX matView = XMMatrixTranspose(XMMatrixLookAtLH(Position.ToXM(), (Position + Forward).ToXM(), Up.ToXM()));
	XMMATRIX matProj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(DegreesToRadians(FOV), aspectRatio, 0.1f, 1000.0f));
	
	WorldToView = matView;

	CameraCB cameraCB{};
	cameraCB.WorldToView = XMUtility::ToXMFloat4x4(matView);
	cameraCB.ViewToClip = XMUtility::ToXMFloat4x4(matProj);
	cameraCB.Position = Position.ToXMF();

	if (!CameraBuffer.Valid()) CameraBuffer = GFX::CreateConstantBuffer<CameraCB>();
	GFX::Cmd::UploadToBuffer(context, CameraBuffer, 0, &cameraCB, 0, sizeof(CameraCB));
}

Light Light::CreateDirectional(Float3 direction, Float3 color)
{
	Light l{};
	l.Type = LT_Directional;
	l.Direction = direction;
	l.Strength = color;
	return l;
}

Light Light::CreateAmbient(Float3 color)
{
	Light l{};
	l.Type = LT_Ambient;
	l.Strength = color;
	return l;
}

Light Light::CreatePoint(Float3 position, Float3 color, Float2 falloff)
{
	Light l{};
	l.Type = LT_Point;
	l.Position = position;
	l.Strength = color;
	l.Falloff = falloff;
	return l;
}

SceneGraph::SceneGraph() :
	Entities(MAX_ENTITIES, sizeof(EntitySB)),
	Materials(MAX_DRAWABLES, sizeof(MaterialSB)),
	Meshes(MAX_DRAWABLES, 0),
	Drawables(MAX_DRAWABLES, sizeof(DrawableSB))
{

}

void SceneGraph::FrameUpdate(ID3D11DeviceContext* context)
{
	MainCamera.UpdateBuffer(context);

	auto f = [&context, this](Drawable d)
	{
		Materials[d.MaterialIndex].UpdateBuffer(context);
		d.UpdateBuffer(context);
	};
	DrawablesToUpdate.ForEachAndClear(f);
}

void SceneGraph::UpdateRenderData(ID3D11DeviceContext* context)
{
	MainCamera.UpdateBuffer(context);

	Entities.Initialize();
	Materials.Initialize();
	Meshes.Initialize();
	Drawables.Initialize();

	Geometries.Initialize();

	Textures = GFX::CreateTextureArray(TEXTURE_SIZE, TEXTURE_SIZE, MAX_TEXTURES, RCF_Bind_SRV | RCF_CopyDest, TEXTURE_MIPS);

	// Lights
	{
		using namespace DirectX;
		struct LightSB
		{
			uint32_t LightType;
			XMFLOAT3 Position;
			XMFLOAT3 Strength;
			XMFLOAT2 Falloff;
			XMFLOAT3 Direction;
			float SpotPower;
		};

		// TODO: Resize buffer when new lights are added and update its data
		if (!LightsBuffer.Valid()) LightsBuffer = GFX::CreateBuffer(sizeof(LightSB) * Lights.size(), sizeof(LightSB), RCF_Bind_SB | RCF_CPU_Write);
		
		ASSERT(GFX::GetNumElements(LightsBuffer) == Lights.size(), "TODO: Resize buffer when new lights are added and update its data");

		std::vector<LightSB> sbLights;
		sbLights.resize(Lights.size());
		uint32_t index = 0;
		for (const Light& l : Lights)
		{
			sbLights[index].LightType = l.Type;
			sbLights[index].Position = l.Position.ToXMF();
			sbLights[index].Strength = l.Strength.ToXMF();
			sbLights[index].Falloff = l.Falloff.ToXMF();
			sbLights[index].Direction = l.Direction.ToXMF();
			sbLights[index].SpotPower = l.SpotPower;
			index++;
		}

		GFX::Cmd::UploadToBuffer(context, LightsBuffer, 0, sbLights.data(), 0, sbLights.size() * sizeof(LightSB));
	}
}

Entity& SceneGraph::CreateEntity(ID3D11DeviceContext* context, Float3 position, Float3 scale)
{
	const uint32_t eIndex = Entities.Next();

	Entity& e = Entities[eIndex];
	e.Index = eIndex;
	e.Position = position;
	e.Scale = scale;
	e.UpdateBuffer(context);

	return Entities[eIndex];
}

Drawable SceneGraph::CreateDrawable(ID3D11DeviceContext* context, Material& material, Mesh& mesh, const Entity& entity)
{
	const uint32_t dIndex = Drawables.Next();
	const uint32_t matIndex = Materials.Next();
	const uint32_t mIndex = Meshes.Next();

	Drawable drawable;
	drawable.DrawableIndex = dIndex;
	drawable.MaterialIndex = matIndex;
	drawable.MeshIndex = mIndex;
	drawable.EntityIndex = entity.Index;

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
	m_IndexBuffer = GFX::CreateBuffer(GetIndexBufferStride(), GetIndexBufferStride(), RCF_Bind_IB | RCF_CopyDest);
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
	GFX::ExpandBuffer(context, m_IndexBuffer, wantedIBSize * GetIndexBufferStride());

	return alloc;
}