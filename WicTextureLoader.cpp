#include "stdafx.h"
#include "WicTextureLoader.h"

#include <wincodec.h>
#include <vector>
#include <comdef.h>
#include "d3dx12.h"
#include <wrl/client.h> 

#pragma comment(lib, "windowscodecs.lib")
 
using Microsoft::WRL::ComPtr;
namespace INANOA {
	namespace Assets
	{
		static bool WicLoadRGBA8(const std::wstring& filePath, std::vector<uint8_t>& outPixels, UINT& outW, UINT& outH)
		{
			ComPtr<IWICImagingFactory> factory;
			if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
				return false;

			ComPtr<IWICBitmapDecoder> decoder;
			if (FAILED(factory->CreateDecoderFromFilename(filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)))
				return false;

			ComPtr<IWICBitmapFrameDecode> frame;
			if (FAILED(decoder->GetFrame(0, &frame)))
				return false;

			UINT w = 0, h = 0;
			frame->GetSize(&w, &h);

			ComPtr<IWICFormatConverter> converter;
			if (FAILED(factory->CreateFormatConverter(&converter)))
				return false;

			if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
				return false;

			outPixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
			const UINT stride = w * 4;
			if (FAILED(converter->CopyPixels(nullptr, stride, (UINT)outPixels.size(), outPixels.data())))
				return false;

			outW = w;
			outH = h;
			return true;
		}

		bool CreateTextureFromFileWIC(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			const std::wstring& filePath,
			Microsoft::WRL::ComPtr<ID3D12Resource>& outTex,
			Microsoft::WRL::ComPtr<ID3D12Resource>& outUpload)
		{
			std::vector<uint8_t> pixels;
			UINT w = 0, h = 0;
			if (!WicLoadRGBA8(filePath, pixels, w, h))
				return false;

			auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, w, h);

			if (FAILED(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&outTex))))
				return false;

			const UINT64 uploadSize = GetRequiredIntermediateSize(outTex.Get(), 0, 1);
			if (FAILED(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&outUpload))))
				return false;

			D3D12_SUBRESOURCE_DATA sub = {};
			sub.pData = pixels.data();
			sub.RowPitch = static_cast<LONG_PTR>(w) * 4;
			sub.SlicePitch = sub.RowPitch * h;

			UpdateSubresources(cmdList, outTex.Get(), outUpload.Get(), 0, 0, 1, &sub);
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outTex.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			return true;
		}
	}
}