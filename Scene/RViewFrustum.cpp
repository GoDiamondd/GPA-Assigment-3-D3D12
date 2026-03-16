#include "stdafx.h"
#include "RViewFrustum.h"
#include <glm/gtc/type_ptr.hpp>

// D3D12 Includes
#include "d3dx12.h" 

using namespace Microsoft::WRL;

namespace INANOA {
	namespace SCENE {
		RViewFrustum::RViewFrustum(const int numCascade, const Camera* camera) :
			m_numCascade(numCascade),
			m_numVertex((numCascade + 1) * 4),
			m_numIndex((numCascade * 4 + (numCascade + 1) * 4) * 2),
			m_modelMat(1.0f)
		{
			this->m_vertexShadowBuffer.resize(m_numVertex * 3, 0.0f);
		}

		RViewFrustum::~RViewFrustum() {
		}

		void RViewFrustum::init(ID3D12Device* device, const Camera* camera) {
			const int NUM_VERTEX = this->m_numVertex;
			const int NUM_CASCADE = this->m_numCascade;
			const int NUM_INDEX = this->m_numIndex;

			// 1. Generate Index Data (CPU)
			std::vector<unsigned int> indices(NUM_INDEX);
			unsigned int indexBufferOffset = 0;
			for (unsigned int i = 0; i < NUM_CASCADE + 1; i++) {
				const unsigned int currLayerStartIdx = i * 4;
				const unsigned int nextLayerStartIdx = (i + 1) * 4;

				if (i < NUM_CASCADE) {
					for (unsigned int j = 0; j < 4; j++) {
						indices[indexBufferOffset++] = nextLayerStartIdx + j;
						indices[indexBufferOffset++] = currLayerStartIdx + j;
					}
				}

				for (unsigned int j = 0; j < 3; j++) {
					indices[indexBufferOffset++] = currLayerStartIdx + j;
					indices[indexBufferOffset++] = currLayerStartIdx + j + 1;
				}
				indices[indexBufferOffset++] = currLayerStartIdx + 3;
				indices[indexBufferOffset++] = currLayerStartIdx + 0;
			}

			// 2. Create GPU Index Buffer
			const UINT indexBufferSize = sizeof(unsigned int) * NUM_INDEX;
			if (FAILED(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_indexBufferResource))))
			{
				return;
			}

			void* pIndexDataBound;
			m_indexBufferResource->Map(0, nullptr, &pIndexDataBound);
			memcpy(pIndexDataBound, indices.data(), indexBufferSize);
			m_indexBufferResource->Unmap(0, nullptr);

			m_indexBufferView.BufferLocation = m_indexBufferResource->GetGPUVirtualAddress();
			m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			m_indexBufferView.SizeInBytes = indexBufferSize;

			// 3. Create GPU Vertex Buffer
			const UINT vertexBufferSize = sizeof(float) * NUM_VERTEX * 3;
			if (FAILED(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_vertexBufferResource))))
			{
				return;
			}

			CD3DX12_RANGE readRange(0, 0);
			m_vertexBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&m_pVertexDataBegin));

			m_vertexBufferView.BufferLocation = m_vertexBufferResource->GetGPUVirtualAddress();
			m_vertexBufferView.StrideInBytes = sizeof(float) * 3;
			m_vertexBufferView.SizeInBytes = vertexBufferSize;
		}

		void RViewFrustum::render(ID3D12GraphicsCommandList* commandList) {
			//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
			commandList->IASetIndexBuffer(&m_indexBufferView);

			// Transform using Inverse View Matrix to place local View Space vertices into World Space
			commandList->SetGraphicsRoot32BitConstants(2, 16, glm::value_ptr(this->m_modelMat), 0);
			commandList->DrawIndexedInstanced(this->m_numIndex, 1, 0, 0, 0);
		}

		void RViewFrustum::update(const Camera* camera) {
			// Update Model Matrix: Inverse View Matrix places the camera-local geometry into the world.
			this->m_modelMat = glm::inverse(camera->viewMatrix());
		}

		void RViewFrustum::resize(const Camera* camera) {
			const float depths[] = {
				camera->near(), camera->far()
			};

			if (this->m_vertexShadowBuffer.size() < this->m_numVertex * 3) {
				this->m_vertexShadowBuffer.resize(this->m_numVertex * 3);
			}

			float* bufferPtr = this->m_vertexShadowBuffer.data();

			for (int i = 0; i < this->m_numCascade + 1; i++) {
				// Use the camera's existing helper to get corners
				camera->viewFrustumClipPlaneCornersInViewSpace(depths[i], bufferPtr + i * 12);

				// FIX: The Camera class helper assumes Right-Handed (-Z) convention internally
				// (hardcoded -1 * depth). D3D12 uses Left-Handed (+Z) forward.
				// We flip the Z coordinate of the generated vertices to correct this orientation.
				for (int k = 0; k < 4; ++k) {
					// Index of Z component: (Layer Offset) + (Vertex Offset) + Z_Index
					int zIndex = (i * 12) + (k * 3) + 2;
					bufferPtr[zIndex] = -bufferPtr[zIndex];
				}
			}

			if (m_pVertexDataBegin) {
				memcpy(m_pVertexDataBegin, bufferPtr, sizeof(float) * this->m_numVertex * 3);
			}
		}
	}
}