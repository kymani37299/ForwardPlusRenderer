#include "samplers.h"
#include "full_screen.h"

struct BloomInput
{
    float4 SampleScale;
    float2 TexelSize;
    float2 Padding;
    float Treshold;
    float Knee;
    float2 Padding2;
};

cbuffer Constants : register(b0)
{
    BloomInput BloomIN;
    float Exposure;
}

VS_IMPL;

float4 DownsampleBox(Texture2D tex, SamplerState samplerTex, float2 uv, float2 texelSize)
{
    float4 A = tex.Sample(samplerTex, uv + texelSize * float2(-1.0, -1.0));
    float4 B = tex.Sample(samplerTex, uv + texelSize * float2(0.0, -1.0));
    float4 C = tex.Sample(samplerTex, uv + texelSize * float2(1.0, -1.0));
    float4 D = tex.Sample(samplerTex, uv + texelSize * float2(-0.5, -0.5));
    float4 E = tex.Sample(samplerTex, uv + texelSize * float2(0.5, -0.5));
    float4 F = tex.Sample(samplerTex, uv + texelSize * float2(-1.0, 0.0));
    float4 G = tex.Sample(samplerTex, uv);
    float4 H = tex.Sample(samplerTex, uv + texelSize * float2(1.0, 0.0));
    float4 I = tex.Sample(samplerTex, uv + texelSize * float2(-0.5, 0.5));
    float4 J = tex.Sample(samplerTex, uv + texelSize * float2(0.5, 0.5));
    float4 K = tex.Sample(samplerTex, uv + texelSize * float2(-1.0, 1.0));
    float4 L = tex.Sample(samplerTex, uv + texelSize * float2(0.0, 1.0));
    float4 M = tex.Sample(samplerTex, uv + texelSize * float2(1.0, 1.0));

    float2 div = (1.0 / 4.0) * float2(0.5, 0.125);

    float4 o = (D + E + I + J) * div.x;
    o += (A + B + G + F) * div.y;
    o += (B + C + H + G) * div.y;
    o += (F + G + L + K) * div.y;
    o += (G + H + M + L) * div.y;

    return o;
}

float4 UpsampleTent(Texture2D tex, SamplerState samplerTex, float2 uv, float2 texelSize, float4 sampleScale)
{
    float4 d = texelSize.xyxy * float4(1.0, 1.0, -1.0, 0.0) * sampleScale;

    float4 s;
    s =  tex.Sample(samplerTex, uv - d.xy);
    s += tex.Sample(samplerTex, uv - d.wy) * 2.0;
    s += tex.Sample(samplerTex, uv - d.zy);
    s += tex.Sample(samplerTex, uv + d.zw) * 2.0;
    s += tex.Sample(samplerTex, uv) * 4.0;
    s += tex.Sample(samplerTex, uv + d.xw) * 2.0;
    s += tex.Sample(samplerTex, uv + d.zy);
    s += tex.Sample(samplerTex, uv + d.wy) * 2.0;
    s += tex.Sample(samplerTex, uv + d.xy);

    return s * (1.0 / 16.0);
}

static const float EPSILON = 0.0001f;

#ifdef PREFILTER

static const float4 EMISSIVE_CLAMP = 20.0f;

Texture2D<float4> inputTexture : register(t0);

float4 QuadraticThreshold(float4 color, float threshold, float3 curve)
{
    // Pixel brightness
    float br = max(max(color.r, color.g), color.b);

    // Under-threshold part: quadratic curve
    float rq = clamp(br - curve.x, 0.0, curve.y);
    rq = curve.z * rq * rq;

    // Combine and apply the brightness response curve.
    color *= max(rq, br - threshold) / max(br, EPSILON);

    return color;
}

float4 PS(FCVertex IN) : SV_Target
{
    const float knee = BloomIN.Knee;
    const float3 curve = float3(BloomIN.Treshold - knee, 2.0f * knee, 0.25f / knee);

    float4 color = DownsampleBox(inputTexture, s_LinearBorder, IN.uv, BloomIN.TexelSize);
    color *= Exposure;
    color = min(EMISSIVE_CLAMP, color);
    color = QuadraticThreshold(color, BloomIN.Treshold, curve);
    return color;
}

#endif // PREFILTER

#ifdef DOWNSAMPLE

Texture2D<float4> inputTexture : register(t0);

float4 PS(FCVertex IN) : SV_Target
{
    return DownsampleBox(inputTexture, s_LinearBorder, IN.uv, BloomIN.TexelSize);
}

#endif // DOWNSAMPLE

#ifdef UPSAMPLE

Texture2D<float4> lowRes : register(t0);
Texture2D<float4> highRes : register(t1);

float4 PS(FCVertex IN) : SV_Target
{
    const float4 bloom = UpsampleTent(lowRes, s_LinearBorder, IN.uv, BloomIN.TexelSize, BloomIN.SampleScale);
    const float4 color = highRes.Sample(s_LinearBorder, IN.uv);
    return bloom + color;
}

#endif // UPSAMPLE