
RWTexture2D<float4> OutputTexture : register(u0);

[numthreads(32, 32, 1)]
void CS(uint3 threadID : SV_DispatchThreadID)
{
	uint2 texCoord = threadID.xy;
	float2 uv = texCoord / 128.0;
	float d = length(uv - float2(0.5, 0.5));
	float col = smoothstep(0.3, 0.35, d);
	OutputTexture[texCoord] = float4(col, 0.0, 0.0, 1.0);
}