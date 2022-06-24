struct FCVertexIN
{
	float2 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct FCVertex
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

FCVertex GetFCVertex(FCVertexIN IN)
{
	FCVertex OUT;
	OUT.pos = float4(IN.pos, 0.0f, 1.0f);
	OUT.uv = IN.uv;
	return OUT;
}

#define VS_IMPL FCVertex VS(FCVertexIN IN) { return GetFCVertex(IN); }