#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED 
#include <vector>
#include <d3d12.h> // Include D3D12 types
#include <string>
#include "../Rendering/RendererBase.h"
#include "../Scene/RViewFrustum.h"
#include "../Scene/RHorizonGround.h"

// Forward define D3D12 types for cleaner headers if desired, 
// though including <d3d12.h> is standard practice.

namespace INANOA {
	class RenderingOrderExp
	{
	public:
		RenderingOrderExp();
		virtual ~RenderingOrderExp();

	public:
		// init requires the D3D12 device to create internal resources
		bool init(ID3D12Device* device, const int w, const int h);

		void resize(const int w, const int h);

		void update();

		// render requires the command list to record commands, and handles to the active back buffer
		void render(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);

		Camera* godCamera() const { return m_godCamera; }
		Camera* playerCamera() const { return m_playerCamera; }
		// One-time asset upload that requires a command list
		bool initGrassAssets(ID3D12GraphicsCommandList* uploadCmd);
		bool initGrassField(ID3D12GraphicsCommandList* uploadCmd, float halfSize, float spacing);
		bool initGrassFieldFromPoisson(ID3D12GraphicsCommandList* uploadCmd, const std::string& ss2Path, float y, float scale);
		bool initAllVegetationFieldsFromPoisson(ID3D12GraphicsCommandList* uploadCmd, float y, float scale);
		bool initVegetationAssets(ID3D12GraphicsCommandList* uploadCmd);

		// Access to renderer (optional but handy)
		D3D12::RendererBase* rendererBase() const { return m_renderer; }
	private:
		SCENE::RViewFrustum* m_viewFrustum = nullptr;
		SCENE::EXPERIMENTAL::HorizonGround* m_horizontalGround = nullptr;

		Camera* m_playerCamera = nullptr;
		Camera* m_godCamera = nullptr;

		glm::vec3 m_cameraForwardMagnitude;
		float m_cameraForwardSpeed;

		int m_frameWidth;
		int m_frameHeight;

		// Updated to D3D12 namespace
		D3D12::RendererBase* m_renderer = nullptr;
	};

}