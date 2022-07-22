struct VertOUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

SamplerState smp : register(s0);
Texture2D src : register(t0);

VertOUT VS(float2 pos : SV_POSITION, float2 uv : TEXCOORD)
{
	VertOUT OUT;
	OUT.pos = float4(pos, 0.0f, 1.0f);
	OUT.uv = uv;
	return OUT;
}

float4 PS(VertOUT IN) : SV_Target
{
	return src.Sample(smp, IN.uv);
}