#include "stdafx.h"

#include "RHorizonGround.h"
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <d3d12.h>
#include <wrl/client.h>
#include <d3dx12.h>

using namespace Microsoft::WRL;

namespace INANOA {
	namespace SCENE {
		namespace EXPERIMENTAL {

			HorizonGround::HorizonGround(const int numCascade, const Camera* camera) :
				// Treat as "number of quads". For a floor, we only need 1 quad.
				/*m_numCascade(1),
				m_numVertex(4),
				m_numIndex(6),
				m_height(0.0f),
				m_modelMat(1.0f)
			{
				this->m_vertexShadowBuffer.resize(m_numVertex * 3, 0.0f);
			}*/
				m_numCascade(numCascade),
				m_numVertex(numCascade * 4),
				m_numIndex(numCascade * 6),
				m_height(0.0f),
				m_modelMat(1.0f)
			{
				this->m_vertexShadowBuffer.resize(static_cast<size_t>(m_numVertex) * 3, 0.0f);
			}

			HorizonGround::~HorizonGround() {
			}

			void HorizonGround::init(ID3D12Device* device, const Camera* camera) {
				const int NUM_VERTEX = this->m_numVertex;
				const int NUM_CASCADE = this->m_numCascade;
				const int NUM_INDEX = this->m_numIndex;

				// 1. Create Index Buffer Data (same as your earlier D3D12 port)
				std::vector<unsigned int> indices(static_cast<size_t>(NUM_INDEX));
				for (unsigned int i = 0; i < static_cast<unsigned int>(NUM_CASCADE); i++) {
					indices[i * 6 + 0] = i * 4 + 0;
					indices[i * 6 + 1] = i * 4 + 1;
					indices[i * 6 + 2] = i * 4 + 2;

					indices[i * 6 + 3] = i * 4 + 2;
					indices[i * 6 + 4] = i * 4 + 3;
					indices[i * 6 + 5] = i * 4 + 0;
				}
				const UINT indexBufferSize = sizeof(unsigned int) * static_cast<UINT>(indices.size());

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

				void* pIndexDataBound = nullptr;
				m_indexBufferResource->Map(0, nullptr, &pIndexDataBound);
				memcpy(pIndexDataBound, indices.data(), indexBufferSize);
				m_indexBufferResource->Unmap(0, nullptr);

				m_indexBufferView.BufferLocation = m_indexBufferResource->GetGPUVirtualAddress();
				m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
				m_indexBufferView.SizeInBytes = indexBufferSize;

				// 2. Vertex buffer (mapped upload)
				const UINT vertexBufferSize = sizeof(float) * static_cast<UINT>(NUM_VERTEX) * 3;

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

			void HorizonGround::render(ID3D12GraphicsCommandList* commandList) {
				//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
				commandList->IASetIndexBuffer(&m_indexBufferView);

				commandList->SetGraphicsRoot32BitConstants(2, 16, glm::value_ptr(this->m_modelMat), 0);

				commandList->DrawIndexedInstanced(static_cast<UINT>(m_numIndex), 1, 0, 0, 0);
			}

			void HorizonGround::resize(const Camera* camera) {
				const float n = camera->near();
				const float f = camera->far();

				const float depths[] = {
					n,
					n + 0.4f * (f - n),
					f,
				};

				float* bufferPtr = this->m_vertexShadowBuffer.data();

				for (int i = 0; i < this->m_numCascade; i++) {
					float* cascadeVertices = bufferPtr + i * 12;

					camera->viewFrustumClipPlaneCornersInViewSpace(depths[i], this->m_cornerBuffer);

					// Z (flip RH->LH)
					cascadeVertices[5] = -1.0f * this->m_cornerBuffer[2];   // v1.z
					cascadeVertices[8] = -1.0f * this->m_cornerBuffer[11];  // v2.z

					camera->viewFrustumClipPlaneCornersInViewSpace(depths[i + 1], this->m_cornerBuffer);

					// X (NO flip)
					cascadeVertices[0] = this->m_cornerBuffer[0];  // v0.x
					cascadeVertices[3] = this->m_cornerBuffer[0];  // v1.x
					cascadeVertices[6] = this->m_cornerBuffer[9];  // v2.x
					cascadeVertices[9] = this->m_cornerBuffer[9];  // v3.x

					// Z (flip RH->LH)
					cascadeVertices[2] = -1.0f * this->m_cornerBuffer[2];   // v0.z
					cascadeVertices[11] = -1.0f * this->m_cornerBuffer[11];  // v3.z

					// Y all zero (ground in view space)
					cascadeVertices[1] = 0.0f;
					cascadeVertices[4] = 0.0f;
					cascadeVertices[7] = 0.0f;
					cascadeVertices[10] = 0.0f;
				}

				if (m_pVertexDataBegin) {
					memcpy(m_pVertexDataBegin, bufferPtr, sizeof(float) * 3 * static_cast<size_t>(this->m_numVertex));
				}
			}

			void HorizonGround::update(const Camera* camera) {
				const glm::vec3 viewPos = camera->viewOrig();
				const glm::mat4 viewMat = camera->viewMatrix();

				glm::mat4 tMat = glm::translate(glm::vec3(viewPos.x, this->m_height, viewPos.z));

				glm::mat4 viewT = glm::transpose(viewMat);
				glm::vec3 forward = glm::vec3(viewT[2].x, 0.0f, viewT[2].z);

				if (glm::length(forward) < 0.0001f) forward = glm::vec3(0, 0, -1);
				else forward = glm::normalize(forward);

				glm::vec3 y(0.0f, 1.0f, 0.0f);
				glm::vec3 x = glm::cross(y, forward);
				if (glm::length(x) < 0.0001f) x = glm::vec3(1, 0, 0);
				else x = glm::normalize(x);

				glm::mat4 rMat(1.0f);
				rMat[0] = glm::vec4(x, 0.0f);
				rMat[1] = glm::vec4(y, 0.0f);
				rMat[2] = glm::vec4(forward, 0.0f);
				rMat[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

				this->m_modelMat = tMat * rMat;
			}

		}
	}
}