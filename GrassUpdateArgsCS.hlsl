// cs_5_0

// same buffer as in cull pass, just bound at a different slot for convenience
RWByteAddressBuffer g_argsAndCounter : register(u0);

cbuffer ArgsCB : register(b0)
{
    uint g_indexCountPerInstance;
    uint g_startIndexLocation;
    int g_baseVertexLocation;
    uint g_startInstanceLocation;
};

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint visibleCount = g_argsAndCounter.Load(0);

    const uint argsOffset = 16;

    // D3D12_DRAW_INDEXED_ARGUMENTS (5 dwords)
    g_argsAndCounter.Store(argsOffset + 0, g_indexCountPerInstance);
    g_argsAndCounter.Store(argsOffset + 4, visibleCount);
    g_argsAndCounter.Store(argsOffset + 8, g_startIndexLocation);
    g_argsAndCounter.Store(argsOffset + 12, (uint) g_baseVertexLocation);
    g_argsAndCounter.Store(argsOffset + 16, g_startInstanceLocation);
}