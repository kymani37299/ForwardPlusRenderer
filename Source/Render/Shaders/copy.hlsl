struct VertexInput
{
	float2 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct VertexOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D src : register(t0);

VertexOut VS(VertexInput IN)
{
	VertexOut OUT;
	OUT.pos = float4(IN.pos, 0.0f, 1.0f);
	OUT.uv = IN.uv;
	return OUT;
}

float4 PS(VertexOut IN) : SV_Target
{
	uint3 pixelCoord = uint3(IN.uv * float2(1024, 768), 0);
	return src.Load(pixelCoord);
}