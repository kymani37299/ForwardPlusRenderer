#include "shared_definitions.h"
#include "samplers.h"
#include "util.h"
#include "full_screen.h"

VS_IMPL;

#ifdef SSAO_SAMPLE

struct SSAOSettings
{
    float SampleRadius;
    float Power;
    float DepthBias;
    float Padding;
};

Texture2D<float4> NoiseTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);

cbuffer Constants : register(b0)
{
    SSAOSettings Settings;
    Camera MainCamera;
    float4 Samples[SSAO_KERNEL_SIZE];
    uint2 ScreenSize;
}

float3 GetNormalFromDepth(float2 uv, Camera camera, Texture2D<float> depth, SamplerState smp)
{
    // TODO: Use ddx and ddy functions for faster calculation
    float2 _uv[3];
    _uv[0] = uv;
    _uv[1] = uv + float2(1.0f, 0.0f) / ScreenSize;
    _uv[2] = uv + float2(0.0f, 1.0f) / ScreenSize;

    float3 p[3];

    [unroll]
    for (uint i = 0; i < 3; i++)
    {
        const float d = depth.Sample(smp, _uv[i]);
        p[i] = GetWorldPositionFromDepth(d, _uv[i], camera);
    }

    return normalize(cross(p[2] - p[0], p[1] - p[0]));
}

float PS(FCVertex IN) : SV_Target
{
    const float2 noiseScale = ScreenSize / 4.0f;
    
    const float depth = DepthTexture.Sample(s_PointClamp, IN.uv);
    const float3 position = GetWorldPositionFromDepth(depth, IN.uv, MainCamera);
    const float3 normal = GetNormalFromDepth(IN.uv, MainCamera, DepthTexture, s_PointClamp);
    const float3 randomDir = NoiseTexture.Sample(s_PointClamp, IN.uv * noiseScale).rgb;

    const float3 tangent = normalize(randomDir - normal * dot(randomDir, normal));
    const float3 bitangent = cross(normal, tangent);
    const float3x3 TBN = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    for (uint i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        const float3 samplePosition = position + mul(Samples[i].xyz, TBN) * Settings.SampleRadius;

        float4 sampleClipPos = GetClipPos(samplePosition, MainCamera);
        sampleClipPos /= sampleClipPos.w;
        sampleClipPos.xy = sampleClipPos.xy * 0.5f + 0.5f; // [0-1]
        sampleClipPos.y = 1.0f - sampleClipPos.y;
        
        const float2 sampleDepthUV = sampleClipPos;
        if (sampleDepthUV.x > 0.0f && sampleDepthUV.x < 1.0f && sampleDepthUV.y > 0.0f && sampleDepthUV.y < 1.0f)
        {
            const float sampleDepth = DepthTexture.Sample(s_PointClamp, sampleDepthUV) - Settings.DepthBias;

            const float rangeFactor = smoothstep(0.0f, 1.0f, Settings.SampleRadius / abs(depth - sampleDepth + Settings.DepthBias));
            occlusion += (sampleDepth >= sampleClipPos.z ? 1.0f : 0.0f) * rangeFactor;
        }
    }

    occlusion = 1.0f - (occlusion / (float)SSAO_KERNEL_SIZE);

    return pow(occlusion, Settings.Power);
}

#endif // SSAO_SAMPLE

#ifdef SSAO_BLUR

cbuffer Constants : register(b0)
{
    uint2 ScreenSize;
}

Texture2D<float> SampledSSAO : register(t0);

float PS(FCVertex IN) : SV_Target
{
    const float2 texelSize = 1.0f / float2(ScreenSize);
    float finalSSAO = 0.0f;
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            const float2 readUV = IN.uv + float2(i, j) * texelSize;
            const float value = SampledSSAO.Sample(s_LinearBorder, readUV);
            finalSSAO += value;
        }
    }

    return finalSSAO / 9.0f;
}

#endif // SSAO_BLUR