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
	if(EntityBuffer == BufferID_Invalid) EntityBuffer = GFX::CreateConstantBuffer(sizeof(EntityCB));
	GFX::Cmd::UploadToBuffer(context, EntityBuffer, &entityCB);
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
	};

	float aspectRatio = (float) AppConfig.WindowWidth / AppConfig.WindowHeight;
	Float3 Up = Float3(0.0f, 1.0f, 0.0f);

	CameraCB cameraCB{};
	cameraCB.WorldToView = XMMatrixTranspose(XMMatrixLookAtLH(Position.ToXM(), (Position + Forward).ToXM(), Up.ToXM()));
	cameraCB.ViewToClip = XMMatrixTranspose(XMMatrixPerspectiveFovLH(DegreesToRadians(FOV), aspectRatio, 0.1f, 1000.0f));
	if (CameraBuffer == BufferID_Invalid) CameraBuffer = GFX::CreateConstantBuffer(sizeof(CameraCB));
	GFX::Cmd::UploadToBuffer(context, CameraBuffer, &cameraCB);
}