#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED 
#include <vector>
#include <d3d12.h> // Include D3D12 types

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