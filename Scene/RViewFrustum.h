#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <glm/mat4x4.hpp>

#include "../Rendering/Camera/Camera.h"

namespace INANOA {
	namespace SCENE {
		class RViewFrustum
		{
		public:
			explicit RViewFrustum(const int numCascade, const Camera* camera);
			virtual ~RViewFrustum();

			// Deleted copy constructors
			RViewFrustum(const RViewFrustum&) = delete;
			RViewFrustum(const RViewFrustum&&) = delete;
			RViewFrustum& operator=(const RViewFrustum&) = delete;

		public:
			// Requires Device to create buffers
			void init(ID3D12Device* device, const Camera* camera);

			// Updates the vertex buffer (mapped memory)
			void resize(const Camera* camera);

			// Requires CommandList to record draw calls
			void render(ID3D12GraphicsCommandList* commandList);

			// Helper to update logic-based state (if any)
			void update(const Camera* camera);

		private:
			const int m_numCascade;
			const int m_numVertex;
			const int m_numIndex;

			// D3D12 Resources
			Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBufferResource;
			Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBufferResource;

			D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
			D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

			// Mapped pointer for dynamic vertex updates
			UINT8* m_pVertexDataBegin = nullptr;

			// CPU-side shadow copy for calculations
			std::vector<float> m_vertexShadowBuffer;

			glm::mat4 m_modelMat = glm::mat4(1.0f);
		};
	}
}