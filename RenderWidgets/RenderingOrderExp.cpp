//#define GLM_FORCE_DEPTH_ZERO_TO_ONE 
//#define GLM_FORCE_LEFT_HANDED
// Note: Defines moved to project settings or stdafx ideally, but can stay here if consistent.
#include "stdafx.h"
#include "RenderingOrderExp.h"
#include <d3d12.h> 
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
namespace INANOA {

	RenderingOrderExp::RenderingOrderExp() {
		this->m_cameraForwardSpeed = 0.25f;
		this->m_cameraForwardMagnitude = glm::vec3(0.0f, 0.0f, 0.0f);
		this->m_frameWidth = 64;
		this->m_frameHeight = 64;
	}
	RenderingOrderExp::~RenderingOrderExp() {
		// Cleanup managed pointers
	}

	bool RenderingOrderExp::init(ID3D12Device* device, const int w, const int h) {
		INANOA::D3D12::RendererBase* renderer = new INANOA::D3D12::RendererBase();
		const std::string vsFile = "vertexShader.hlsl";
		const std::string fsFile = "pixelShader.hlsl";

		if (renderer->init(device, vsFile, fsFile, w, h) == false) return false;
		this->m_renderer = renderer;

		this->m_godCamera = new Camera(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 5.0f, 60.0f, 0.1f, 512.0f);
		this->m_godCamera->resize(w, h);
		this->m_godCamera->setViewOrg(glm::vec3(0.0f, 55.0f, 50.0f));
		this->m_godCamera->setLookCenter(glm::vec3(0.0f, 32.0f, -12.0f));
		this->m_godCamera->setDistance(70.0f);
		this->m_godCamera->update();

		this->m_playerCamera = new Camera(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 9.5f, -5.0f), glm::vec3(0.0f, 1.0f, 0.0f), 10.0, 45.0f, 1.0f, 150.0f);
		this->m_playerCamera->resize(w, h);
		this->m_playerCamera->update();

		// Scene Setup
		{
			this->m_viewFrustum = new SCENE::RViewFrustum(1, this->m_playerCamera);
			this->m_viewFrustum->init(device, this->m_playerCamera);
			this->m_viewFrustum->resize(this->m_playerCamera);
			this->m_viewFrustum->update(this->m_playerCamera);

			this->m_horizontalGround = new SCENE::EXPERIMENTAL::HorizonGround(2, this->m_playerCamera);
			this->m_horizontalGround->init(device, this->m_playerCamera);
			this->m_horizontalGround->resize(this->m_playerCamera);
			this->m_horizontalGround->update(this->m_playerCamera);
		}

		this->resize(w, h);
		return true;
	}

	void RenderingOrderExp::resize(const int w, const int h) {
		const int HW = w * 0.5;
		this->m_playerCamera->resize(HW, h);
		this->m_godCamera->resize(HW, h);
		m_renderer->resize(w, h);
		this->m_frameWidth = w;
		this->m_frameHeight = h;
		this->m_viewFrustum->resize(this->m_playerCamera);
		this->m_horizontalGround->resize(this->m_playerCamera);
	}

	void RenderingOrderExp::update() {
		m_godCamera->update();
		this->m_playerCamera->forward(this->m_cameraForwardMagnitude, true);
		this->m_playerCamera->update();
		this->m_viewFrustum->update(this->m_playerCamera);
		this->m_horizontalGround->update(this->m_playerCamera);
	}

	void RenderingOrderExp::render(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) {

		// 1. IMPORTANT: Bind Render Targets before drawing!
		//commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		//// 2. Clear buffers (Manually via command list in D3D12)
		//const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		//commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		//commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		const int HW = this->m_frameWidth * 0.5;

		// Set Scissor (Full screen)
		D3D12_RECT scissor = { 0, 0, (LONG)this->m_frameWidth, (LONG)this->m_frameHeight };
		commandList->RSSetScissorRects(1, &scissor);

		// =====================================================
		// God View (Left side)
		// =====================================================
		this->m_renderer->setCamera(
			commandList,
			m_godCamera->projMatrix(),
			m_godCamera->viewMatrix(),
			m_godCamera->viewOrig()
		);
		
		this->m_renderer->setViewport(commandList, 0, 0, HW, this->m_frameHeight);

		// Draw Ground
		this->m_renderer->useLinePSO(commandList, false);
		// Draw Grass (God View)
		this->m_renderer->setShadingModel(commandList, D3D12::ShadingModelType::PROCEDURAL_GRID);
		
		this->m_horizontalGround->render(commandList);

		// Draw Frustum
		this->m_renderer->setShadingModel(commandList, D3D12::ShadingModelType::UNLIT);
		const glm::mat4 playerVP = this->m_playerCamera->projMatrix() * this->m_playerCamera->viewMatrix();
		// For each vegetation type: cull (optional) then draw indirect
		for (size_t i = 0; i < 3; ++i) // 0=grassB, 1=bush01, 2=bush05
		{
			// If you want compute-cull each frame:
			m_renderer->cullVegetationToViewFrustumCS(commandList, i, playerVP);

			// Draw using ExecuteIndirect; uses visible buffer if computeReady, else candidates.
			m_renderer->drawVegetationIndirect(commandList, i);
		}
		this->m_renderer->setCamera(
			commandList,
			m_godCamera->projMatrix(),
			m_godCamera->viewMatrix(),
			m_godCamera->viewOrig()
		);
		this->m_renderer->useLinePSO(commandList, true);
		
		this->m_viewFrustum->render(commandList);
		// =====================================================
		// Player View (Right side)
		// =====================================================

		// Clear Depth for the 2nd viewport
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		this->m_renderer->setCamera(
			commandList,
			this->m_playerCamera->projMatrix(),
			this->m_playerCamera->viewMatrix(),
			this->m_playerCamera->viewOrig()
		);

		this->m_renderer->setViewport(commandList, HW, 0, HW, this->m_frameHeight);
		this->m_renderer->setShadingModel(commandList, D3D12::ShadingModelType::PROCEDURAL_GRID);
		this->m_renderer->useLinePSO(commandList, false);
		this->m_horizontalGround->render(commandList);

	
		//this->m_renderer->cullGrassToViewFrustumCS(commandList, playerVP);

		//this->m_renderer->drawGrassIndirect(commandList);
		// For each vegetation type: cull (optional) then draw indirect
		for (size_t i = 0; i < 3; ++i) // 0=grassB, 1=bush01, 2=bush05
		{
			// Draw using ExecuteIndirect; uses visible buffer if computeReady, else candidates.
			m_renderer->drawVegetationIndirect(commandList, i);
		}


	}

	bool RenderingOrderExp::initGrassFieldFromPoisson(ID3D12GraphicsCommandList* uploadCmd, const std::string& ss2Path, float y, float scale)
	{
		if (!m_renderer)
			return false;

		return m_renderer->initGrassFieldFromPoisson(uploadCmd, ss2Path, y, scale);
	}

	/*bool RenderingOrderExp::initGrassField(ID3D12GraphicsCommandList* uploadCmd, float halfSize, float spacing)
	{
		if (!m_renderer)
			return false;

		return m_renderer->initGrassField(uploadCmd, halfSize, spacing);
	}*/
	bool INANOA::RenderingOrderExp::initAllVegetationFieldsFromPoisson(ID3D12GraphicsCommandList* uploadCmd, float y, float scale)
	{
		if (!m_renderer)
			return false;

		return m_renderer->initAllVegetationFieldsFromPoisson(uploadCmd, y, scale);
	}
	bool RenderingOrderExp::initGrassAssets(ID3D12GraphicsCommandList* uploadCmd)
	{
		if (!m_renderer)
			return false;

		return m_renderer->initGrassAssets(uploadCmd);
	}

	bool RenderingOrderExp::initVegetationAssets(ID3D12GraphicsCommandList* uploadCmd)
	{
		if (!m_renderer)
			return false;

		return m_renderer->initVegetationAssets(uploadCmd);
	}
}