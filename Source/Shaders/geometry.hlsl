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

SamplerState LinearClamp : register(s0);
Texture2D AlbedoTexture : register(t0);

VertexOut VS(VertexInput IN)
{
	VertexOut OUT;
	OUT.Position = float4(IN.Position, 1.0f);
	OUT.UV = IN.UV;
	return OUT;
}

float4 PS(VertexOut IN) : SV_Target
{
	return AlbedoTexture.Sample(LinearClamp, IN.UV);
}