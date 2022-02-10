struct VertexInput
{
	float2 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
};

VertexOut VS(VertexInput IN)
{
	VertexOut OUT;
	OUT.pos = 0.5f * float4(IN.pos, 0.0f, 1.0f);
	return OUT;
}

float4 PS(VertexOut IN) : SV_Target
{
	return float4(1.0f, 0.0f, 0.0f, 1.0f);
}