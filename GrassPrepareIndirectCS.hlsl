cbuffer PrepareCB : register(b0)
{
    uint gIndexCountPerInstance;
    uint gStartIndexLocation;
    int gBaseVertexLocation;
    uint gStartInstanceLocation;
    uint gMaxInstances;
};

RWByteAddressBuffer gIndirectArgs : register(u0);
RWByteAddressBuffer gCounter : register(u1);

// D3D12_DRAW_INDEXED_ARGUMENTS byte offsets
static const uint ARGS_IndexCountPerInstance = 0;
static const uint ARGS_InstanceCount = 4;
static const uint ARGS_StartIndexLocation = 8;
static const uint ARGS_BaseVertexLocation = 12;
static const uint ARGS_StartInstanceLocation = 16;

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    // reset counter (uint)
    gCounter.Store(0, 0);

    // write constant parts of args
    gIndirectArgs.Store(ARGS_IndexCountPerInstance, gIndexCountPerInstance);
    gIndirectArgs.Store(ARGS_InstanceCount, 0);
    gIndirectArgs.Store(ARGS_StartIndexLocation, gStartIndexLocation);
    gIndirectArgs.Store(ARGS_BaseVertexLocation, asuint(gBaseVertexLocation));
    gIndirectArgs.Store(ARGS_StartInstanceLocation, gStartInstanceLocation);
}