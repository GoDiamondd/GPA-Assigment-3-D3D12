#pragma once
#include <string>
#include <wrl/client.h>
#include <d3d12.h>

namespace INANOA {
	namespace Assets {
		
		// Creates a DEFAULT heap RGBA8 texture + an UPLOAD heap staging buffer, and records copy commands to cmdList.
		// Caller must execute cmdList and keep upload resource alive until GPU finishes.
		bool CreateTextureFromFileWIC(
			ID3D12Device * device,
			ID3D12GraphicsCommandList * cmdList,
			const std::wstring & filePath,
			Microsoft::WRL::ComPtr<ID3D12Resource>&outTex,
			Microsoft::WRL::ComPtr<ID3D12Resource>&outUpload);
	}
}