struct GrassInstance
{
    float3 pos;
    float scale;
    float yaw;
    float pad0;
};

cbuffer FrameCB : register(b0)
{
    float4x4 gViewProj;
    float4 gFrustumPlanes[6]; // unused in VS/PS
    uint gMaxInstances;
    uint gDisableCulling;
    float3 _pad;
};

StructuredBuffer<GrassInstance> gAllInstances : register(t0);
StructuredBuffer<uint> gVisibleIndices : register(t1);
Texture2D gDiffuse : register(t2);
SamplerState gSamp : register(s0);

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float2x2 Rot2(float a)
{
    float s = sin(a);
    float c = cos(a);
    return float2x2(c, -s, s, c);
}

PSIn VSMain(VSIn vin, uint instanceId : SV_InstanceID)
{
    PSIn o;

    uint instIndex = (gDisableCulling != 0) ? instanceId : gVisibleIndices[instanceId];
    GrassInstance inst = gAllInstances[instIndex];

    float2x2 r = Rot2(inst.yaw);
    float2 xz = mul(r, vin.pos.xz * inst.scale);

    float3 worldPos = float3(xz.x, vin.pos.y * inst.scale, xz.y) + inst.pos;

    o.pos = mul(gViewProj, float4(worldPos, 1.0f));
    o.uv = vin.uv;
    return o;
}

float4 PSMain(PSIn pin) : SV_Target
{
    return gDiffuse.Sample(gSamp, pin.uv);
}