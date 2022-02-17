#include "SceneGraph.h"

#include "Render/Commands.h"
#include "Render/Buffer.h"
#include "System/ApplicationConfiguration.h"

SceneGraph MainSceneGraph;

namespace
{
	float DegreesToRadians(float deg)
	{
		const float DEG_2_RAD = 3.1415f / 180.0f;
		return deg * DEG_2_RAD;
	}
}

void Material::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;
	struct MaterialCB
	{
		XMFLOAT3A AlbedoFactor;
		XMFLOAT3A FresnelR0;
		float MetallicFactor;
		float RoughnessFactor;
	};

	MaterialCB matCB{};
	matCB.AlbedoFactor = AlbedoFactor.ToXMFA();
	matCB.FresnelR0 = FresnelR0.ToXMFA();
	matCB.MetallicFactor = MetallicFactor;
	matCB.RoughnessFactor = RoughnessFactor;
	if (!MaterialParams.Valid()) MaterialParams = GFX::CreateConstantBuffer<MaterialCB>();
	GFX::Cmd::UploadToBuffer(context, MaterialParams, &matCB, sizeof(MaterialCB));
}

void Entity::UpdateBuffer(ID3D11DeviceContext* context)
{
	using namespace DirectX;
	struct EntityCB
	{
		XMFLOAT4X4 ModelToWorld;
	};

	XMMATRIX modelToWorld = XMMatrixTranspose(XMMatrixAffineTransformation(Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), Position.ToXM()));
	EntityCB entityCB{};
	entityCB.ModelToWorld = XMUtility::ToXMFloat4x4(modelToWorld);
	if(!EntityBuffer.Valid()) EntityBuffer = GFX::CreateConstantBuffer<EntityCB>();
	GFX::Cmd::UploadToBuffer(context, EntityBuffer, &entityCB, sizeof(EntityCB));

	const auto func = [&context](Drawable& d) { d.Material.UpdateBuffer(context); };
	Drawables.ForEach(func);
}

namespace
{
	void RotToAxis(Float3 rot, Float3& forward, Float3& up, Float3& right)
	{
		using namespace DirectX;

		// TODO: Calculate up based on roll
		up = Float3(0.0f, 1.0f, 0.0f);
		forward = Float3((float)(std::cos(rot.y) * std::cos(rot.x)), (float)(std::sin(rot.x)), (float)(std::sin(rot.y) * std::cos(rot.x)));
		
		XMVECTOR vec = XMVector3Cross(forward.ToXM(), up.ToXM());
		vec = XMVector3Normalize(vec);
		right = Float3(vec);
	}
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
	GFX::Cmd::UploadToBuffer(context, CameraBuffer, &cameraCB, sizeof(CameraCB));
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

void SceneGraph::UpdateRenderData(ID3D11DeviceContext* context)
{
	MainCamera.UpdateBuffer(context);

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

		GFX::Cmd::UploadToBuffer(context, LightsBuffer, sbLights.data(), sbLights.size() * sizeof(LightSB));
	}
}