struct VertexInput
{
	float3 Position : SV_POSITION;
	float2 UV : TEXCOORD;
	float3 Normal : NORMAL;
	float4 Tangent : TANGENT;
};

struct VertexOut
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD;
};

struct CameraData
{
	float4x4 View;
	float4x4 Projection;
};

cbuffer CameraCB : register(b0)
{
	CameraData CamData;
}

SamplerState LinearClamp : register(s0);
Texture2D AlbedoTexture : register(t0);

VertexOut VS(VertexInput IN)
{
	const float4 modelPos = float4(IN.Position, 1.0f);
	const float4 worldPos = modelPos * 0.01f; // TODO: Add transform
	const float4 viewPos = mul(worldPos, CamData.View);
	const float4 clipPos = mul(viewPos, CamData.Projection);

	VertexOut OUT;
	OUT.Position = clipPos;
	OUT.UV = IN.UV;
	return OUT;
}

float4 PS(VertexOut IN) : SV_Target
{
	return AlbedoTexture.Sample(LinearClamp, IN.UV);
}