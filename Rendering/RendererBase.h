#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED 
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <glm/mat4x4.hpp>
#include <string>
#include <glm/gtc/type_ptr.hpp> 

// This project typically requires the d3dx12 helper header. 
// If you do not have "d3dx12.h", you must obtain it from Microsoft's repo 
// or use standard D3D12 structs (which makes init code much simpler).
#include "d3dx12.h" 

namespace INANOA {
	namespace D3D12 {
		enum class ShadingModelType : int {
			PROCEDURAL_GRID = 0,
			UNLIT = 5
		};

		class RendererBase
		{
		public:
			explicit RendererBase();
			virtual ~RendererBase();

			RendererBase(const RendererBase&) = delete;
			RendererBase(const RendererBase&&) = delete;
			RendererBase& operator=(const RendererBase&) = delete;

		public:
			// D3D12 Init: Requires Device pointer and RTV format to build the PSO
			bool init(ID3D12Device* device, const std::string& vsResource, const std::string& fsResource, const int width, const int height, DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM);

			// Resize just updates internal resolution state; SwapChain resize happens externally
			void resize(const int w, const int h);

			// Updated: Requires CommandList to record root constants
			void setCamera(ID3D12GraphicsCommandList* commandList, const glm::mat4& projMat, const glm::mat4& viewMat, const glm::vec3& viewOrg);

			// Updated: Requires CommandList and specific handles to the buffers to clear
			void clearRenderTarget(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);

		public:
			// These inline helpers now require the command list to function

			inline void setShadingModel(ID3D12GraphicsCommandList* commandList, const ShadingModelType type) {
				int data[4] = { static_cast<int>(type), 0, 0, 0 }; // Full 16 bytes
				// Use the PLURAL function to set multiple constants
				commandList->SetGraphicsRoot32BitConstants(3, 4, data, 0);
			}

			inline void setViewport(ID3D12GraphicsCommandList* commandList, const int x, const int y, const int w, const int h) {
				D3D12_VIEWPORT viewport = { (float)x, (float)y, (float)w, (float)h, 0.0f, 1.0f };
				D3D12_RECT scissor = { x, y, x + w, y + h }; // Approximate scissor to viewport
				commandList->RSSetViewports(1, &viewport);
				commandList->RSSetScissorRects(1, &scissor);
			}

			// clearDepth is usually handled via clearRenderTarget in D3D12 by passing the DSV handle

		public:
			// Helper to switch between Triangle and Line drawing modes
			// Pass 'true' to draw lines (Frustum), 'false' for triangles (Ground)
			void useLinePSO(ID3D12GraphicsCommandList* commandList, bool useLines);

		private:
			glm::mat4 m_viewMat;
			glm::mat4 m_projMat;
			glm::vec4 m_viewPosition;

			int m_frameWidth = 64;
			int m_frameHeight = 64;

			// D3D12 Pipeline Objects
			// Replaces "ShaderProgram* m_shaderProgram"
			Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
			Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;     // Triangles
			Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineStateLine; // Add this for Lines!
			//Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineStateLineOverlay;

			// Weak pointer to device (owned by the main window class)
			ID3D12Device* m_device = nullptr;
		};
	}
}