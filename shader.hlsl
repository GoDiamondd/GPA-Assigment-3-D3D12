// shaders.hlsl - updated to sample diffuse texture using t0 and static sampler s0

cbuffer SceneCB : register(b0)
{
    float4x4 mvp;
};

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 pos = float4(input.position, 1.0f);
    output.position = mul(pos, mvp);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

Texture2D diffuseTex : register(t0);
SamplerState sampLinear : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 tex = diffuseTex.Sample(sampLinear, input.uv);

    // Alpha test (cutout) to avoid blending/z-order issues for overlapping transparent textures.
    // Pixels with alpha below the threshold are discarded so depth remains correct.
    clip(tex.a - 0.5);

    // Multiply sampled color by vertex-derived color (normals->color) for simple lighting
    float3 finalColor = tex.rgb;
    return float4(finalColor, 1.0f);
}