#pragma once

#include <d3d12.h>
#include <wrl/client.h>

extern Microsoft::WRL::ComPtr<ID3D12CommandQueue> g_queue;
extern Microsoft::WRL::ComPtr<ID3D12Fence> g_fence;
extern HANDLE g_fenceEvent;
extern UINT64 g_fenceValue;