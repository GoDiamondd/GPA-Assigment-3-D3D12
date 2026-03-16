#include "stdafx.h"
#include "RendererBase.h"
// Ensure you have including the D3D12 headers in your RendererBase.h or here
// #include <d3d12.h>
// #include <d3dcompiler.h> 
// #include "d3dx12.h" // Helper structure library (highly recommended for D3D12)
#include <iostream>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;

namespace INANOA {
	namespace D3D12 {

		RendererBase::RendererBase() {
			this->m_viewMat = glm::mat4x4(1.0f);
			this->m_projMat = glm::mat4x4(1.0f);
			this->m_viewPosition = glm::vec4(0.0f);
		}

		RendererBase::~RendererBase() {
			// D3D12 objects are released automatically via ComPtr types in the header
		}

		// Helper for compiling shaders
		HRESULT CompileShader(const std::wstring& filename, const char* entryPoint, const char* profile, ID3DBlob** outBlob)
		{
			ComPtr<ID3DBlob> errorBlob;
			UINT compileFlags = 0;
#if defined(_DEBUG)
			compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
			// Note: This assumes vsResource/fsResource are filenames. 
			// If they are raw source strings, use D3DCompile instead of D3DCompileFromFile.
			HRESULT hr = D3DCompileFromFile(filename.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, profile, compileFlags, 0, outBlob, &errorBlob);

			if (FAILED(hr) && errorBlob)
			{
				OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			}
			return hr;
		}

		// Note: init signature modified to take the Device. D3D12 objects cannot exist without a Device.
		bool RendererBase::init(ID3D12Device* device, const std::string& vsPath, const std::string& fsPath, const int width, const int height, DXGI_FORMAT rtvFormat) {
			m_device = device;
			m_frameWidth = width;
			m_frameHeight = height;

			// 1. Create Root Signature
			// We define 2 slots for 32-bit root constants (perfect for small data like matrices).
			// Slot 0: View Matrix (16 floats)
			// Slot 1: Proj Matrix (16 floats)
			// Within RendererBase::init(...)

			// 1. Create Root Signature
			{
				// CHANGE: Increase count from 2 to 4
				CD3DX12_ROOT_PARAMETER rootParameters[4];

				// Slot 0: View Matrix (b0)
				rootParameters[0].InitAsConstants(16, 0);

				// Slot 1: Projection Matrix (b1)
				rootParameters[1].InitAsConstants(16, 1);

				// Slot 2: Model Matrix (b2) - ADD THIS
				rootParameters[2].InitAsConstants(16, 2);

				// Slot 3: Shading Param / Misc (b3) - ADD THIS (matches pixelShader.hlsl)
				// We pass 4 ints/floats (16 bytes) to cover struct { int id; float3 padding; }
				rootParameters[3].InitAsConstants(4, 3);

				CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
				// CHANGE: Pass 4 as the count
				rootSignatureDesc.Init(4, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
					if (error) OutputDebugStringA((char*)error->GetBufferPointer());
					return false;
				}
				if (FAILED(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)))) {
					return false;
				}
			}

			// 2. Compile Shaders and Create Pipeline State (PSO)
			{
				ComPtr<ID3DBlob> vertexShader;
				ComPtr<ID3DBlob> pixelShader;

				// Convert std::string to std::wstring for D3D API
				std::wstring wVsPath(vsPath.begin(), vsPath.end());
				std::wstring wFsPath(fsPath.begin(), fsPath.end());

				if (FAILED(CompileShader(wVsPath, "VSMain", "vs_5_0", &vertexShader))) return false;
				if (FAILED(CompileShader(wFsPath, "PSMain", "ps_5_0", &pixelShader))) return false;

				// Define Input Layout (Must match your vertex struct)
				D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					// Add others like normal/texcoord here if your vertex shader typically uses them
				};

				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
				psoDesc.pRootSignature = m_rootSignature.Get();
				psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
				psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
				psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Often safer for starting out
				psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				psoDesc.DepthStencilState.DepthEnable = TRUE;
				psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
				psoDesc.DepthStencilState.StencilEnable = FALSE;
				psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = rtvFormat; // e.g., DXGI_FORMAT_R8G8B8A8_UNORM
				psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // Assuming a depth buffer exists
				psoDesc.SampleDesc.Count = 1;

				if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)))) {
					return false;
				}

				// Create the Line PSO (Reuse the same compiled shaders!)
				{
					D3D12_GRAPHICS_PIPELINE_STATE_DESC linePsoDesc = psoDesc; // Copy settings
					
					// CRITICAL CHANGE: Set topology type to LINE
					linePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
					linePsoDesc.DepthStencilState.DepthEnable = TRUE;
					linePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
					linePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
					linePsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
					if (FAILED(m_device->CreateGraphicsPipelineState(&linePsoDesc, IID_PPV_ARGS(&m_pipelineStateLine)))) {
						return false;
					}
				}

				// Create the Overlay Line PSO (always visible)
				/*{
					D3D12_GRAPHICS_PIPELINE_STATE_DESC overlayDesc = psoDesc;
					overlayDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

					overlayDesc.DepthStencilState.DepthEnable = FALSE;
					overlayDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

					if (FAILED(m_device->CreateGraphicsPipelineState(&overlayDesc, IID_PPV_ARGS(&m_pipelineStateLineOverlay))))
						return false;
				}*/
			}

			return true;
		}

		// D3D12 requires the CommandList to set state.
		void RendererBase::setCamera(ID3D12GraphicsCommandList* commandList, const glm::mat4& projMat, const glm::mat4& viewMat, const glm::vec3& viewOrg) {
			this->m_projMat = projMat;
			this->m_viewMat = viewMat;
			this->m_viewPosition = glm::vec4(viewOrg, 1.0f);

			// In OpenGL `glUniform` pushes to the active program.
			// In D3D12, we must bind the PSO and Root Signature, then push constants.

			commandList->SetGraphicsRootSignature(m_rootSignature.Get()); // Ensure root sig is set
			//commandList->SetPipelineState(m_pipelineState.Get());         // Ensure PSO is set

			// Upload matrices directly to the Root Signature via Root Constants
			// This is very fast but limited in size (max 64 DWORDS). 2 matrices = 32 DWORDS.
			commandList->SetGraphicsRoot32BitConstants(0, 16, glm::value_ptr(this->m_viewMat), 0);
			commandList->SetGraphicsRoot32BitConstants(1, 16, glm::value_ptr(this->m_projMat), 0);
		}

		void RendererBase::resize(const int w, const int h) {
			this->m_frameWidth = w;
			this->m_frameHeight = h;

			// In D3D12, simple resize variable updates aren't enough.
			// The SwapChain must be resized externally, and RTVs recreated.
			// We only store the data here.
		}

		// In D3D12, "Clear" needs to know WHICH buffer to clear (Handle) and use a Command List
		void RendererBase::clearRenderTarget(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) {
			static const float color[] = { 0.0f, 0.0f, 0.0f, 1.0f };

			commandList->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		}

		// Add the implementation for the helper
		void RendererBase::useLinePSO(ID3D12GraphicsCommandList* commandList, bool useLines)
		{
			if (useLines)
			{
				/*ID3D12PipelineState* pso = nullptr;

				if (overlay && m_pipelineStateLineOverlay)
					pso = m_pipelineStateLineOverlay.Get();
				else
					pso = ;*/

				commandList->SetPipelineState(m_pipelineStateLine.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			}
			else
			{
				commandList->SetPipelineState(m_pipelineState.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			}
		}
	}
}