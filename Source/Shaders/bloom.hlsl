#include "samplers.h"
#include "full_screen.h"

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

// TODO: Add exposure
static const float EXPOSURE = 1.0f;

// x: threshold value (linear), y: threshold - knee, z: knee * 2, w: 0.25 / knee
static const float4 EMISSIVE_TRESHOLD = float4(1.0f, 0.75f, 1.5f, 0.25 / 1.5f);
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
    // TODO: Use CB for this
    uint3 textureSize = 0;
    inputTexture.GetDimensions(0, textureSize.x, textureSize.y, textureSize.z);
    const float2 texelSize = 1.0f / textureSize.xy;

    float4 color = DownsampleBox(inputTexture, s_LinearWrap, IN.uv, texelSize);
    color *= EXPOSURE;
    color = min(EMISSIVE_CLAMP, color);
    color = QuadraticThreshold(color, EMISSIVE_TRESHOLD.x, EMISSIVE_TRESHOLD.yzw);
    return color;
}

#endif // PREFILTER

#ifdef DOWNSAMPLE

Texture2D<float4> inputTexture : register(t0);

float4 PS(FCVertex IN) : SV_Target
{
    // TODO: Use CB for this
    uint3 textureSize = 0;
    inputTexture.GetDimensions(0, textureSize.x, textureSize.y, textureSize.z);
    const float2 texelSize = 1.0f / textureSize.xy;

    return DownsampleBox(inputTexture, s_LinearWrap, IN.uv, texelSize);
}

#endif // DOWNSAMPLE

#ifdef UPSAMPLE

static const float SAMPLE_SCALE = float4(1.0f, 1.0f, 1.0f, 1.0f);

Texture2D<float4> lowRes : register(t0);
Texture2D<float4> highRes : register(t1);

float4 PS(FCVertex IN) : SV_Target
{
    // TODO: Use CB for this
    uint3 textureSize = 0;
    lowRes.GetDimensions(0, textureSize.x, textureSize.y, textureSize.z);
    const float2 texelSize = 1.0f / textureSize.xy;

    const float4 bloom = UpsampleTent(lowRes, s_LinearWrap, IN.uv, texelSize, SAMPLE_SCALE);
    const float4 color = highRes.Sample(s_LinearWrap, IN.uv);

    return bloom + color;
}

#endif // UPSAMPLE