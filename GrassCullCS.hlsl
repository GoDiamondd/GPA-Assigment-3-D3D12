struct GrassInstance
{
    float4 r0;
    float4 r1;
    float4 r2;
    float4 r3;
};

cbuffer CullCB : register(b0)
{
    float4x4 gViewProj;
    uint gCandidateCount;
    uint gIndexCountPerInstance;
    uint _pad0;
    uint _pad1;
};

StructuredBuffer<float4x4> gCandidates : register(t0);
RWStructuredBuffer<float4x4> gVisible : register(u0);
RWStructuredBuffer<uint> gVisibleCount : register(u1);
RWByteAddressBuffer gIndirectArgs : register(u2); // 20 bytes = D3D12_DRAW_INDEXED_ARGUMENTS

[numthreads(256, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint i = tid.x;
    if (i >= gCandidateCount)
        return;

    float4x4 model = gCandidates[i];

    // Robust world position of instance origin (translation)
    float3 worldPos = mul(model, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;

    float4 clip = mul(gViewProj, float4(worldPos, 1.0f));
    if (clip.w <= 0.0f)
        return;

    float3 ndc = clip.xyz / clip.w;

    if (ndc.x < -1.0f || ndc.x > 1.0f ||
        ndc.y < -1.0f || ndc.y > 1.0f ||
        ndc.z < 0.0f || ndc.z > 1.0f)
        return;

    uint outIndex;
    InterlockedAdd(gVisibleCount[0], 1, outIndex);

    // write out the same data type layout as candidates (float4x4)
    gVisible[outIndex] = model;
}

[numthreads(1, 1, 1)]
void CSFinalize(uint3 tid : SV_DispatchThreadID)
{
    uint count = gVisibleCount[0];

    // D3D12_DRAW_INDEXED_ARGUMENTS:
    // 0: IndexCountPerInstance (uint)
    // 4: InstanceCount (uint)
    // 8: StartIndexLocation (uint)
    // 12: BaseVertexLocation (int)
    // 16: StartInstanceLocation (uint)
    gIndirectArgs.Store(0, gIndexCountPerInstance);
    gIndirectArgs.Store(4, count);
    gIndirectArgs.Store(8, 0);
    gIndirectArgs.Store(12, 0);
    gIndirectArgs.Store(16, 0);
}