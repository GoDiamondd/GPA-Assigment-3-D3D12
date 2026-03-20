#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <glm/mat4x4.hpp>
#include <string>
#include <glm/gtc/type_ptr.hpp>
#include "d3dx12.h"
#include <vector>

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
            bool init(ID3D12Device* device, const std::string& vsResource, const std::string& fsResource, const int width, const int height, DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
            void resize(const int w, const int h);
            void setCamera(ID3D12GraphicsCommandList* commandList, const glm::mat4& projMat, const glm::mat4& viewMat, const glm::vec3& viewOrg);
            void clearRenderTarget(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);

        public:
            inline void setShadingModel(ID3D12GraphicsCommandList* commandList, const ShadingModelType type) {
                int data[4] = { static_cast<int>(type), 0, 0, 0 };
                commandList->SetGraphicsRoot32BitConstants(3, 4, data, 0);
            }

            inline void setViewport(ID3D12GraphicsCommandList* commandList, const int x, const int y, const int w, const int h) {
                D3D12_VIEWPORT viewport = { (float)x, (float)y, (float)w, (float)h, 0.0f, 1.0f };
                D3D12_RECT scissor = { x, y, x + w, y + h };
                commandList->RSSetViewports(1, &viewport);
                commandList->RSSetScissorRects(1, &scissor);
            }

        public:
            void useLinePSO(ID3D12GraphicsCommandList* commandList, bool useLines);

        public:
            // NEW multi-vegetation API
            bool initVegetationAssets(ID3D12GraphicsCommandList* uploadCmd);
            bool initVegetationFieldFromPoisson(ID3D12GraphicsCommandList* uploadCmd, size_t typeIndex, const std::string& ss2Path, float y, float scale);
            void cullVegetationToViewFrustumCS(ID3D12GraphicsCommandList* commandList, size_t typeIndex, const glm::mat4& viewProj);
            void drawVegetationIndirect(ID3D12GraphicsCommandList* commandList, size_t typeIndex);
            bool initAllVegetationFieldsFromPoisson(ID3D12GraphicsCommandList* uploadCmd, float y, float scale);
            // Backward-compatible wrappers (optional: keep callers unchanged)
            bool initGrassAssets(ID3D12GraphicsCommandList* uploadCmd) { return initVegetationAssets(uploadCmd); }
            bool initGrassFieldFromPoisson(ID3D12GraphicsCommandList* uploadCmd, const std::string& ss2Path, float y, float scale) { return initVegetationFieldFromPoisson(uploadCmd, 0, ss2Path, y, scale); }
            void cullGrassToViewFrustumCS(ID3D12GraphicsCommandList* commandList, const glm::mat4& viewProj) { cullVegetationToViewFrustumCS(commandList, 0, viewProj); }
            void drawGrassIndirect(ID3D12GraphicsCommandList* commandList) { drawVegetationIndirect(commandList, 0); }

        private:
            glm::mat4 m_viewMat;
            glm::mat4 m_projMat;
            glm::vec4 m_viewPosition;

            int m_frameWidth = 64;
            int m_frameHeight = 64;

            Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
            Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
            Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineStateLine;

            Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignatureTextured;
            Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineStateTextured;

            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
            UINT m_srvInc = 0;

            struct GrassVertex
            {
                float px, py, pz;
                float nx, ny, nz;
                float u, v;
            };

            struct GrassInstance { float m[16]; };

            struct VegetationType
            {
                std::string name;

                std::string objPath;
                std::string poissonSs2Path;
                // Geometry
                Microsoft::WRL::ComPtr<ID3D12Resource> vb;
                Microsoft::WRL::ComPtr<ID3D12Resource> ib;
                D3D12_VERTEX_BUFFER_VIEW vbv{};
                D3D12_INDEX_BUFFER_VIEW ibv{};
                UINT indexCount = 0;

                // Texture SRV (slotBase + 0)
                Microsoft::WRL::ComPtr<ID3D12Resource> tex;
                Microsoft::WRL::ComPtr<ID3D12Resource> texUpload;
                D3D12_GPU_DESCRIPTOR_HANDLE texSrvGpu{};

                // Candidate instances SRV (slotBase + 1)
                Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer;
                Microsoft::WRL::ComPtr<ID3D12Resource> instanceUpload;
                D3D12_GPU_DESCRIPTOR_HANDLE instanceSrvGpu{};
                UINT instanceCount = 0;

                // CPU-built indirect args (fallback draw; slotless)
                Microsoft::WRL::ComPtr<ID3D12Resource> indirectArgs;
                Microsoft::WRL::ComPtr<ID3D12Resource> indirectArgsUpload;

                // Compute-cull outputs:
                // visible instances SRV (slotBase + 2)
                // visible instances UAV (slotBase + 3)
                // visible count UAV (slotBase + 4)
                // indirect args UAV (slotBase + 5)
                Microsoft::WRL::ComPtr<ID3D12Resource> visibleInstances;     // default heap, UAV+SRV
                Microsoft::WRL::ComPtr<ID3D12Resource> visibleCount;         // default heap, UAV uint[1]
                Microsoft::WRL::ComPtr<ID3D12Resource> visibleCountUpload;   // upload heap, uint=0
                Microsoft::WRL::ComPtr<ID3D12Resource> indirectArgsCS;       // default heap, UAV+INDIRECT

                D3D12_GPU_DESCRIPTOR_HANDLE visibleSrvGpu{};
                D3D12_GPU_DESCRIPTOR_HANDLE visibleUavGpu{};
                D3D12_GPU_DESCRIPTOR_HANDLE visibleCountUavGpu{};
                D3D12_GPU_DESCRIPTOR_HANDLE indirectArgsCsvGpu{};

                bool computeReady = false;

                UINT heapSlotBase = 0; // starting slot for this type (6 descriptors per type)
            };

            // fixed set for now: grassB, bush01, bush05
            std::vector<VegetationType> m_veg;

            ID3D12Device* m_device = nullptr;

            Microsoft::WRL::ComPtr<ID3D12CommandSignature> m_cmdSigDrawIndexed;

            Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignatureComputeGrass;
            Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoComputeGrassCull;
            Microsoft::WRL::ComPtr<ID3D12PipelineState> m_psoComputeGrassFinalize;
        };

    } // namespace D3D12
} // namespace INANOA