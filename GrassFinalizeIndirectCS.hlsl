RWByteAddressBuffer gIndirectArgs : register(u0);
RWByteAddressBuffer gCounter : register(u1);

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint count = gCounter.Load(0);

    // D3D12_DRAW_INDEXED_ARGUMENTS:
    // 0: IndexCountPerInstance
    // 4: InstanceCount  <-- write this
    // 8: StartIndexLocation
    // 12: BaseVertexLocation
    // 16: StartInstanceLocation
    gIndirectArgs.Store(4, count);
}