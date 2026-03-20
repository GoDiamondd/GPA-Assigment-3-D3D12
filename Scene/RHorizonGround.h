#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <glm/mat4x4.hpp>

#include "../Rendering/Camera/Camera.h"

namespace INANOA {
	namespace SCENE {
		namespace EXPERIMENTAL {
			class HorizonGround
			{
			public:
				explicit HorizonGround(const int numCascade, const Camera* camera);
				virtual ~HorizonGround();

				HorizonGround(const HorizonGround&) = delete;
				HorizonGround(const HorizonGround&&) = delete;
				HorizonGround& operator=(const HorizonGround&) = delete;

			public:
				// Requires Device to create buffers
				void init(ID3D12Device* device, const Camera* camera);

				// Standard logic update
				void update(const Camera* camera);

				// Updates vertex data in the mapped Upload Buffer
				void resize(const Camera* camera);

				// Records draw commands to the list
				void render(ID3D12GraphicsCommandList* commandList);

			private:
				const int m_numCascade;
				const int m_numVertex;
				const int m_numIndex;
				const float m_height;

				// D3D12 Resources
				Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBufferResource;
				Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBufferResource;

				// Views to bind to the pipeline
				D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
				D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

				// Pointer to the mapped UPLOAD buffer memory for dynamic updates
				UINT8* m_pVertexDataBegin = nullptr;

				// CPU-side copy of vertex data for calculations (Replaces original float* m_vertexBuffer)
				std::vector<float> m_vertexShadowBuffer;

				float m_cornerBuffer[12];

				glm::mat4 m_modelMat;
			};
		}
	}
}