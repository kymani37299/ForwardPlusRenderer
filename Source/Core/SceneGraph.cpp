#include "SceneGraph.h"

#include "Render/Commands.h"
#include "Render/Buffer.h"
#include "System/ApplicationConfiguration.h"

namespace
{
	float DegreesToRadians(float deg)
	{
		const float DEG_2_RAD = 3.1415f / 180.0f;
		return deg * DEG_2_RAD;
	}
}

void Entity::UpdateBuffer(ID3D11DeviceContext1* context)
{
	using namespace DirectX;
	struct EntityCB
	{
		XMMATRIX ModelToWorld;
	};

	EntityCB entityCB{};
	entityCB.ModelToWorld = XMMatrixTranspose(XMMatrixAffineTransformation(Scale.ToXM(), Float3(0.0f, 0.0f, 0.0f).ToXM(), Float4(0.0f, 0.0f, 0.0f, 0.0f).ToXM(), Position.ToXM()));
	if(EntityBuffer == BufferID_Invalid) EntityBuffer = GFX::CreateConstantBuffer<EntityCB>();
	GFX::Cmd::UploadToBuffer(context, EntityBuffer, &entityCB, sizeof(EntityCB));
}

Camera::Camera(Float3 position, Float3 forward, float fov):
	Position(position),
	Forward(forward),
	FOV(fov)
{ }

void Camera::UpdateBuffer(ID3D11DeviceContext1* context)
{
	using namespace DirectX;
	struct CameraCB
	{
		XMMATRIX WorldToView;
		XMMATRIX ViewToClip;
		Float3 Position;
	};

	float aspectRatio = (float) AppConfig.WindowWidth / AppConfig.WindowHeight;
	Float3 Up = Float3(0.0f, 1.0f, 0.0f);

	CameraCB cameraCB{};
	cameraCB.WorldToView = XMMatrixTranspose(XMMatrixLookAtLH(Position.ToXM(), (Position + Forward).ToXM(), Up.ToXM()));
	cameraCB.ViewToClip = XMMatrixTranspose(XMMatrixPerspectiveFovLH(DegreesToRadians(FOV), aspectRatio, 0.1f, 1000.0f));
	cameraCB.Position = Position;
	if (CameraBuffer == BufferID_Invalid) CameraBuffer = GFX::CreateConstantBuffer<CameraCB>();
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

void SceneGraph::UpdateRenderData(ID3D11DeviceContext1* context)
{
	for (Entity& e : Entities)
	{
		e.UpdateBuffer(context);
	}

	MainCamera.UpdateBuffer(context);

	// Lights
	
	{
		struct LightSB
		{
			uint32_t LightType;
			Float3 Position;
			Float3 Strength;
			Float2 Falloff;
			Float3 Direction;
			float SpotPower;
		};

		// TODO: Resize buffer when new lights are added and update its data
		if (LightsBuffer == BufferID_Invalid) LightsBuffer = GFX::CreateBuffer(sizeof(LightSB) * Lights.size(), sizeof(LightSB), RCF_Bind_SB | RCF_CPU_Write);
		
		ASSERT(GFX::GetNumBufferElements(LightsBuffer) == Lights.size(), "TODO: Resize buffer when new lights are added and update its data");

		std::vector<LightSB> sbLights;
		sbLights.resize(Lights.size());
		uint32_t index = 0;
		for (const Light& l : Lights)
		{
			sbLights[index].LightType = l.Type;
			sbLights[index].Position = l.Position;
			sbLights[index].Strength = l.Strength;
			sbLights[index].Falloff = l.Falloff;
			sbLights[index].Direction = l.Direction;
			sbLights[index].SpotPower = l.SpotPower;
			index++;
		}

		GFX::Cmd::UploadToBuffer(context, LightsBuffer, sbLights.data(), sbLights.size() * sizeof(LightSB));
	}
}