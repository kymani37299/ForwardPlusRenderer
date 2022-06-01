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
	
	struct MeshSB
	{
		uint32_t VertexOffset;
		uint32_t IndexOffset;
	};

	struct DrawableSB
	{
		uint32_t EntityIndex;
		uint32_t MaterialIndex;
		uint32_t MeshIndex;
	};

	struct LightSB
	{
		uint32_t LightType;
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

void Mesh::UpdateBuffer(ID3D11DeviceContext* context, RenderGroup& renderGroup)
{
	using namespace DirectX;

	MeshSB meshSB{};
	meshSB.VertexOffset = VertOffset;
	meshSB.IndexOffset = IndexOffset;
	GFX::Cmd::UploadToBuffer(context, renderGroup.Meshes.GetBuffer(), sizeof(MeshSB) * MeshIndex, &meshSB, 0, sizeof(MeshSB));
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
	drawableSB.MeshIndex = MeshIndex;
	GFX::Cmd::UploadToBuffer(context, renderGroup.Drawables.GetBuffer(), sizeof(DrawableSB) * DrawableIndex, &drawableSB, 0, sizeof(DrawableSB));
}

void Light::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;

	LightSB lightSB{};
	lightSB.LightType = Type;
	lightSB.Position = Position.ToXMF();
	lightSB.Radiance = Radiance.ToXMF();
	lightSB.Falloff = Falloff.ToXMF();
	lightSB.Direction = Direction.ToXMF();
	lightSB.SpotPower = SpotPower;
	GFX::Cmd::UploadToBuffer(context, MainSceneGraph.Lights.GetBuffer(), sizeof(LightSB) * LightIndex, &lightSB, 0, sizeof(LightSB));
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
	cam.ZFar;
	return cam;
}

Float3 CalcForwardVector(float pitch, float yaw)
{
	const float x = std::cosf(yaw) * std::cosf(pitch);
	const float y = std::sinf(pitch);
	const float z = std::sinf(yaw) * std::cos(pitch);
	return Float3{ x,y,z }.Normalize();
}

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
	Meshes(MAX_DRAWABLES, sizeof(MeshSB)),
	Drawables(MAX_DRAWABLES, sizeof(DrawableSB))
{

}

void RenderGroup::Initialize(ID3D11DeviceContext* context)
{
	Materials.Initialize();
	Meshes.Initialize();
	Drawables.Initialize();
	MeshData.Initialize();
	TextureData.Initialize();
}

uint32_t RenderGroup::AddMaterial(ID3D11DeviceContext* context, Material& material)
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

uint32_t RenderGroup::AddMesh(ID3D11DeviceContext* context, Mesh& mesh)
{
	const uint32_t index = Meshes.Next();
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

void RenderGroup::AddDraw(ID3D11DeviceContext* context, uint32_t materialIndex, uint32_t meshIndex, uint32_t entityIndex, const BoundingSphere& boundingSphere)
{
	const uint32_t index = Drawables.Next();
	
	Drawable drawable;
	drawable.DrawableIndex = index;
	drawable.MaterialIndex = materialIndex;
	drawable.MeshIndex = meshIndex;
	drawable.EntityIndex = entityIndex;
	drawable.BoundingVolume = boundingSphere;
	Drawables[index] = drawable;

	if (Device::Get()->IsMainContext(context))
	{
		drawable.UpdateBuffer(context, *this);
	}
	else
	{
		NOT_IMPLEMENTED;
	}
}

void RenderGroup::SetupPipelineInputs(ID3D11DeviceContext* context)
{
	GFX::Cmd::BindSRV<VS|PS>(context, TextureData.GetBuffer(), 120);
	GFX::Cmd::BindSRV<VS|PS>(context, MeshData.GetVertexBuffer(), 121);
	GFX::Cmd::BindSRV<VS|PS>(context, MeshData.GetIndexBuffer(), 122);
	GFX::Cmd::BindSRV<VS|PS>(context, Meshes.GetBuffer(), 123);
	GFX::Cmd::BindSRV<VS|PS>(context, MainSceneGraph.Entities.GetBuffer(), 124);
	GFX::Cmd::BindSRV<VS|PS>(context, Materials.GetBuffer(), 125);
	GFX::Cmd::BindSRV<VS|PS>(context, Drawables.GetBuffer(), 126);
}

SceneGraph::SceneGraph() :
	Entities(MAX_ENTITIES, sizeof(EntitySB)),
	Lights(MAX_LIGHTS, sizeof(LightSB))
{

}

void SceneGraph::InitRenderData(ID3D11DeviceContext* context)
{
	MainCamera = Camera::CreatePerspective(75.0f, (float) AppConfig.WindowWidth / AppConfig.WindowHeight, 0.1f, 1000.0f);
	ShadowCamera = Camera::CreateOrtho(500.0f, 500.0f, -500.0f, 500.0f);
	ShadowCamera.UseRotation = false;
	
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

	// Shadow camera
	{
		ShadowCamera.NextTransform.Position = MainCamera.CurrentTranform.Position;
		if (DirLightIndex != UINT32_MAX)
			ShadowCamera.NextTransform.Forward = Lights[DirLightIndex].Direction;
		ShadowCamera.FrameUpdate(context);
	}

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

uint32_t SceneGraph::AddEntity(ID3D11DeviceContext* context, Entity entity)
{
	const uint32_t index = Entities.Next();
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

Light SceneGraph::CreateDirectionalLight(ID3D11DeviceContext* context, Float3 direction, Float3 color)
{
	ASSERT(DirLightIndex == UINT32_MAX, "Only one directional light is supported per scene!");

	Light l{};
	l.Type = LT_Directional;
	l.Direction = direction;
	l.Radiance = color;

	l = CreateLight(context, l);
	DirLightIndex = l.LightIndex;
	return l;
}

Light SceneGraph::CreateAmbientLight(ID3D11DeviceContext* context, Float3 color)
{
	Light l{};
	l.Type = LT_Ambient;
	l.Radiance = color;
	return CreateLight(context, l);
}

Light SceneGraph::CreatePointLight(ID3D11DeviceContext* context, Float3 position, Float3 color, Float2 falloff)
{
	Light l{};
	l.Type = LT_Point;
	l.Position = position;
	l.Radiance = color;
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
	m_VertexBuffer = GFX::CreateBuffer(GetVertexBufferStride(), GetVertexBufferStride(), RCF_Bind_SB | RCF_CopyDest);
	m_IndexBuffer = GFX::CreateBuffer(GetIndexBufferStride(), GetIndexBufferStride(), RCF_Bind_SB | RCF_CopyDest);
}

MeshStorage::Allocation MeshStorage::Allocate(ID3D11DeviceContext* context, uint32_t vertexCount, uint32_t indexCount)
{
	MeshStorage::Allocation alloc{};
	alloc.VertexOffset = m_VertexCount.fetch_add(vertexCount);
	alloc.IndexOffset = m_IndexCount.fetch_add(indexCount);

	const uint32_t wantedVBSize = vertexCount + alloc.VertexOffset;
	const uint32_t wantedIBSize = indexCount + alloc.IndexOffset;

	GFX::ExpandBuffer(context, m_VertexBuffer, wantedVBSize * GetVertexBufferStride());
	GFX::ExpandBuffer(context, m_IndexBuffer, wantedIBSize * GetIndexBufferStride());

	return alloc;
}

void TextureStorage::Initialize()
{
	m_Data = GFX::CreateTextureArray(TEXTURE_SIZE, TEXTURE_SIZE, MAX_TEXTURES, RCF_Bind_SRV | RCF_CopyDest, TEXTURE_MIPS);
	m_StagingTexture = GFX::CreateTexture(TEXTURE_SIZE, TEXTURE_SIZE, RCF_Bind_RTV | RCF_GenerateMips | RCF_Bind_SRV, TEXTURE_MIPS);
}

TextureStorage::Allocation TextureStorage::AddTexture(ID3D11DeviceContext* context, TextureID texture)
{
	const uint32_t textureIndex = m_NextAllocation++;
	ASSERT(textureIndex < MAX_TEXTURES, "textureIndex < SceneGraph::MAX_TEXTURES");

	const Texture& stagingTex = GFX::Storage::GetTexture(m_StagingTexture);

	// Resize texture and generate mips
	ID3D11DeviceContext* c = context;

	GFX::Cmd::MarkerBegin(c, "CopyTexture");
	GFX::Cmd::SetupStaticSamplers<PS>(c);
	GFX::Cmd::BindShader<VS | PS>(c, Device::Get()->GetCopyShader());
	c->OMSetRenderTargets(1, stagingTex.RTV.GetAddressOf(), nullptr);
	GFX::Cmd::SetViewport(c, TEXTURE_SIZE, TEXTURE_SIZE);
	GFX::Cmd::BindSRV<PS>(c, texture, 0);
	GFX::Cmd::DrawFC(context);
	GFX::Cmd::MarkerEnd(c);

	c->GenerateMips(stagingTex.SRV.Get());

	GFX::Storage::Free(texture);

	// Copy to the array
	const Texture& dataTex = GFX::Storage::GetTexture(m_Data);

	ASSERT(stagingTex.Format == dataTex.Format, "stagingTex.Format == tex.Format");

	for (uint32_t mip = 0; mip < dataTex.NumMips; mip++)
	{
		uint32_t srcSubresource = D3D11CalcSubresource(mip, 0, stagingTex.NumMips);
		uint32_t dstSubresource = D3D11CalcSubresource(mip, textureIndex, dataTex.NumMips);
		c->CopySubresourceRegion(dataTex.Handle.Get(), dstSubresource, 0, 0, 0, stagingTex.Handle.Get(), srcSubresource, nullptr);
	}

	return { textureIndex };
}
