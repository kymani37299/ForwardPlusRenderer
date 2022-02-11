#include "SceneGraph.h"

#include "Render/Commands.h"
#include "Render/Buffer.h"
#include "System/ApplicationConfiguration.h"

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
		XMMATRIX View;
		XMMATRIX Projection;
	};

	float aspectRatio = AppConfig.WindowWidth / AppConfig.WindowHeight;
	Float3 Up = Float3(0.0f, 1.0f, 0.0f);

	CameraCB cameraCB{};
	cameraCB.View = XMMatrixLookAtLH(Position.ToXM(), (Position + Forward).ToXM(), Up.ToXM());
	cameraCB.Projection = XMMatrixPerspectiveFovLH(FOV, aspectRatio, 0.1f, 1000.0f);

	if (CameraBuffer == BufferID_Invalid) CameraBuffer = GFX::CreateConstantBuffer(sizeof(CameraCB));
	GFX::Cmd::UploadToBuffer(context, CameraBuffer, &cameraCB);
}