#include "stdafx.h"
#include "RendererBase.h"
#include <iostream>
#include <vector>
#include "ObjMtlLoader.h"
#include "WicTextureLoader.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#include <fstream>
#include <sstream>
#include <windows.h>
#include "Scene/SpatialSample.h"
using namespace Microsoft::WRL;

namespace INANOA {
	namespace D3D12 {

		RendererBase::RendererBase() {
			this->m_viewMat = glm::mat4x4(1.0f);
			this->m_projMat = glm::mat4x4(1.0f);
			this->m_viewPosition = glm::vec4(0.0f);
		}

		RendererBase::~RendererBase() {
		}

		HRESULT CompileShader(const std::wstring& filename, const char* entryPoint, const char* profile, ID3DBlob** outBlob)
		{
			ComPtr<ID3DBlob> errorBlob;
			UINT compileFlags = 0;
#if defined(_DEBUG)
			compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
			HRESULT hr = D3DCompileFromFile(filename.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, profile, compileFlags, 0, outBlob, &errorBlob);

			if (FAILED(hr) && errorBlob)
			{
				OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			}
			return hr;
		}

		static std::string GetCwdA()
		{
			char buf[MAX_PATH] = {};
			DWORD n = GetCurrentDirectoryA(MAX_PATH, buf);
			return (n > 0) ? std::string(buf) : std::string();
		}

		static bool FileExistsA(const std::string& path)
		{
			std::ifstream f(path.c_str(), std::ios::binary);
			return f.good();
		}

		static std::string ToDbgLine(const std::string& s)
		{
			return s + "\n";
		}

		static std::string BasenameA(const std::string& path)
		{
			const size_t p = path.find_last_of("/\\");
			return (p == std::string::npos) ? path : path.substr(p + 1);
		}

		// Helper: descriptor handle at heap slot
		static D3D12_CPU_DESCRIPTOR_HANDLE CpuHandleAt(ID3D12DescriptorHeap* heap, UINT inc, UINT slot)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
			h.ptr += static_cast<SIZE_T>(inc) * static_cast<SIZE_T>(slot);
			return h;
		}
		static D3D12_GPU_DESCRIPTOR_HANDLE GpuHandleAt(ID3D12DescriptorHeap* heap, UINT inc, UINT slot)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE h = heap->GetGPUDescriptorHandleForHeapStart();
			h.ptr += static_cast<UINT64>(inc) * static_cast<UINT64>(slot);
			return h;
		}

		bool RendererBase::init(ID3D12Device* device, const std::string& vsPath, const std::string& fsPath, const int width, const int height, DXGI_FORMAT rtvFormat)
		{
			m_device = device;
			m_frameWidth = width;
			m_frameHeight = height;

			{
				CD3DX12_ROOT_PARAMETER rootParameters[4];
				rootParameters[0].InitAsConstants(16, 0);
				rootParameters[1].InitAsConstants(16, 1);
				rootParameters[2].InitAsConstants(16, 2);
				rootParameters[3].InitAsConstants(4, 3);

				CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
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

			{
				ComPtr<ID3DBlob> vertexShader;
				ComPtr<ID3DBlob> pixelShader;

				std::wstring wVsPath(vsPath.begin(), vsPath.end());
				std::wstring wFsPath(fsPath.begin(), fsPath.end());

				if (FAILED(CompileShader(wVsPath, "VSMain", "vs_5_0", &vertexShader))) return false;
				if (FAILED(CompileShader(wFsPath, "PSMain", "ps_5_0", &pixelShader))) return false;

				D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				};

				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
				psoDesc.pRootSignature = m_rootSignature.Get();
				psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
				psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
				psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				psoDesc.DepthStencilState.DepthEnable = TRUE;
				psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
				psoDesc.DepthStencilState.StencilEnable = FALSE;
				psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = rtvFormat;
				psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				psoDesc.SampleDesc.Count = 1;

				if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)))) {
					return false;
				}

				{
					D3D12_GRAPHICS_PIPELINE_STATE_DESC linePsoDesc = psoDesc;
					linePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
					linePsoDesc.DepthStencilState.DepthEnable = TRUE;
					linePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
					linePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
					linePsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
					if (FAILED(m_device->CreateGraphicsPipelineState(&linePsoDesc, IID_PPV_ARGS(&m_pipelineStateLine)))) {
						return false;
					}
				}
			}

			// Textured root signature
			{
				CD3DX12_ROOT_PARAMETER rp[5];

				rp[0].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
				rp[1].InitAsConstants(16, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
				rp[2].InitAsConstants(16, 2, 0, D3D12_SHADER_VISIBILITY_VERTEX);

				CD3DX12_DESCRIPTOR_RANGE texRange;
				texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
				rp[3].InitAsDescriptorTable(1, &texRange, D3D12_SHADER_VISIBILITY_PIXEL);

				CD3DX12_DESCRIPTOR_RANGE instRange;
				instRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
				rp[4].InitAsDescriptorTable(1, &instRange, D3D12_SHADER_VISIBILITY_VERTEX);

				CD3DX12_STATIC_SAMPLER_DESC samp(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

				CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
				rsDesc.Init(_countof(rp), rp, 1, &samp, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> sig, err;
				if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err)))
				{
					if (err) OutputDebugStringA((char*)err->GetBufferPointer());
					return false;
				}
				if (FAILED(m_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignatureTextured))))
					return false;
			}

			// Textured PSO
			{
				ComPtr<ID3DBlob> vsTex;
				ComPtr<ID3DBlob> psTex;

				std::wstring wVsPath(vsPath.begin(), vsPath.end());
				std::wstring wFsPath(fsPath.begin(), fsPath.end());

				if (FAILED(CompileShader(wVsPath, "VSMainTextured", "vs_5_0", &vsTex))) return false;
				if (FAILED(CompileShader(wFsPath, "PSMainTextured", "ps_5_0", &psTex))) return false;

				D3D12_INPUT_ELEMENT_DESC layout[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				};

				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
				pso.InputLayout = { layout, _countof(layout) };
				pso.pRootSignature = m_rootSignatureTextured.Get();
				pso.VS = CD3DX12_SHADER_BYTECODE(vsTex.Get());
				pso.PS = CD3DX12_SHADER_BYTECODE(psTex.Get());
				pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				pso.DepthStencilState.DepthEnable = TRUE;
				pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				pso.SampleMask = UINT_MAX;
				pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				pso.NumRenderTargets = 1;
				pso.RTVFormats[0] = rtvFormat;
				pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				pso.SampleDesc.Count = 1;

				if (FAILED(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pipelineStateTextured))))
					return false;
			}

			// SRV heap (CBV/SRV/UAV): 6 descriptors per vegetation type
			{
				const UINT kDescriptorsPerType = 6;
				const UINT kTypeCount = 3; // grassB + bush01 + bush05
				D3D12_DESCRIPTOR_HEAP_DESC hd = {};
				hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				hd.NumDescriptors = kDescriptorsPerType * kTypeCount;
				hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				if (FAILED(m_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_srvHeap))))
					return false;

				m_srvInc = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}

			// Indirect command signature (DrawIndexed)
			{
				D3D12_INDIRECT_ARGUMENT_DESC arg = {};
				arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

				D3D12_COMMAND_SIGNATURE_DESC cs = {};
				cs.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
				cs.NumArgumentDescs = 1;
				cs.pArgumentDescs = &arg;

				if (FAILED(m_device->CreateCommandSignature(&cs, nullptr, IID_PPV_ARGS(&m_cmdSigDrawIndexed))))
					return false;
			}

			// Compute pipeline for vegetation frustum culling (shared)
			{
				CD3DX12_ROOT_PARAMETER rp[5];

				rp[0].InitAsConstants(20, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

				CD3DX12_DESCRIPTOR_RANGE rSrv;
				rSrv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
				rp[1].InitAsDescriptorTable(1, &rSrv, D3D12_SHADER_VISIBILITY_ALL);

				CD3DX12_DESCRIPTOR_RANGE rUav0;
				rUav0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0
				rp[2].InitAsDescriptorTable(1, &rUav0, D3D12_SHADER_VISIBILITY_ALL);

				CD3DX12_DESCRIPTOR_RANGE rUav1;
				rUav1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1); // u1
				rp[3].InitAsDescriptorTable(1, &rUav1, D3D12_SHADER_VISIBILITY_ALL);

				CD3DX12_DESCRIPTOR_RANGE rUav2;
				rUav2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2); // u2
				rp[4].InitAsDescriptorTable(1, &rUav2, D3D12_SHADER_VISIBILITY_ALL);

				CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
				rsDesc.Init(_countof(rp), rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

				ComPtr<ID3DBlob> sig, err;
				if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err)))
				{
					if (err) OutputDebugStringA((char*)err->GetBufferPointer());
					return false;
				}
				if (FAILED(m_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignatureComputeGrass))))
					return false;

				ComPtr<ID3DBlob> csCull;
				ComPtr<ID3DBlob> csFinalize;

				std::wstring csPath = L"GrassCullCS.hlsl";
				if (FAILED(CompileShader(csPath, "CSMain", "cs_5_0", &csCull))) return false;
				if (FAILED(CompileShader(csPath, "CSFinalize", "cs_5_0", &csFinalize))) return false;

				D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};
				cpsd.pRootSignature = m_rootSignatureComputeGrass.Get();

				cpsd.CS = CD3DX12_SHADER_BYTECODE(csCull.Get());
				if (FAILED(m_device->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&m_psoComputeGrassCull))))
					return false;

				cpsd.CS = CD3DX12_SHADER_BYTECODE(csFinalize.Get());
				if (FAILED(m_device->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&m_psoComputeGrassFinalize))))
					return false;
			}

			// Define vegetation types (assets)
			{
				m_veg.clear();
				m_veg.resize(3);

				m_veg[0].name = "grassB";
				m_veg[0].objPath = "assets/models/foliages/grassB.obj";
				m_veg[0].poissonSs2Path = "assets/models/spatialSamples/poissonPoints_155304s.ss2"; 
				m_veg[0].heapSlotBase = 0;

				m_veg[1].name = "bush01";
				m_veg[1].objPath = "assets/models/foliages/bush01_lod2.obj";
				m_veg[1].poissonSs2Path = "assets/models/spatialSamples/poissonPoints_1010s.ss2";
				m_veg[1].heapSlotBase = 6;

				m_veg[2].name = "bush05";
				m_veg[2].objPath = "assets/models/foliages/bush05_lod2.obj";
				m_veg[2].poissonSs2Path = "assets/models/spatialSamples/poissonPoints_2797s.ss2";
				m_veg[2].heapSlotBase = 12;
			}

			return true;
		}

		bool RendererBase::initAllVegetationFieldsFromPoisson(ID3D12GraphicsCommandList* uploadCmd, float y, float scale)
		{
			if (!uploadCmd || m_veg.empty())
				return false;

			for (size_t i = 0; i < m_veg.size(); ++i)
			{
				const std::string& ss2 = m_veg[i].poissonSs2Path;
				if (ss2.empty())
					return false;

				if (!initVegetationFieldFromPoisson(uploadCmd, i, ss2, y, scale))
					return false;
			}
			return true;
		}

		bool RendererBase::initVegetationAssets(ID3D12GraphicsCommandList* uploadCmd)
		{
			if (!m_device || !uploadCmd || !m_srvHeap || m_veg.empty())
				return false;

			OutputDebugStringA(ToDbgLine("[Veg] CWD: " + GetCwdA()).c_str());

			for (size_t i = 0; i < m_veg.size(); ++i)
			{
				VegetationType& v = m_veg[i];

				INANOA::Assets::ObjMesh mesh;
				OutputDebugStringA(ToDbgLine("[Veg] Loading " + v.name + " OBJ: " + v.objPath).c_str());
				OutputDebugStringA(ToDbgLine(std::string("[Veg] OBJ exists? ") + (FileExistsA(v.objPath) ? "YES" : "NO")).c_str());

				if (!INANOA::Assets::LoadObjWithMtl(v.objPath, mesh))
				{
					OutputDebugStringA(ToDbgLine("[Veg] LoadObjWithMtl failed for " + v.name).c_str());
					return false;
				}

				if (mesh.vertices.empty() || mesh.indices.empty())
				{
					OutputDebugStringA(ToDbgLine("[Veg] OBJ produced no geometry for " + v.name).c_str());
					return false;
				}
				if (mesh.diffuseTexturePath.empty())
				{
					OutputDebugStringA(ToDbgLine("[Veg] No diffuseTexturePath for " + v.name).c_str());
					return false;
				}

				std::vector<GrassVertex> verts;
				verts.reserve(mesh.vertices.size());
				for (const auto& vv : mesh.vertices)
				{
					GrassVertex gv;
					gv.px = vv.pos.x; gv.py = vv.pos.y; gv.pz = vv.pos.z;
					gv.nx = vv.nrm.x; gv.ny = vv.nrm.y; gv.nz = vv.nrm.z;
					gv.u = vv.uv.x; gv.v = vv.uv.y;
					verts.push_back(gv);
				}

				v.indexCount = static_cast<UINT>(mesh.indices.size());

				const UINT vbSize = static_cast<UINT>(verts.size() * sizeof(GrassVertex));
				const UINT ibSize = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));

				// Geometry buffers (upload heap for simplicity, consistent with existing code)
				if (FAILED(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(vbSize),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&v.vb))))
					return false;

				if (FAILED(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(ibSize),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&v.ib))))
					return false;

				void* p = nullptr;
				v.vb->Map(0, nullptr, &p);
				memcpy(p, verts.data(), vbSize);
				v.vb->Unmap(0, nullptr);

				v.ib->Map(0, nullptr, &p);
				memcpy(p, mesh.indices.data(), ibSize);
				v.ib->Unmap(0, nullptr);

				v.vbv.BufferLocation = v.vb->GetGPUVirtualAddress();
				v.vbv.StrideInBytes = sizeof(GrassVertex);
				v.vbv.SizeInBytes = vbSize;

				v.ibv.BufferLocation = v.ib->GetGPUVirtualAddress();
				v.ibv.Format = DXGI_FORMAT_R32_UINT;
				v.ibv.SizeInBytes = ibSize;

				// Texture
				std::string texPathA = mesh.diffuseTexturePath;
				if (!FileExistsA(texPathA))
				{
					const std::string fileOnly = BasenameA(texPathA);
					const std::string fallback = std::string("assets/textures/") + fileOnly;
					OutputDebugStringA(ToDbgLine("[Veg] Remap texture -> " + fallback).c_str());
					if (FileExistsA(fallback))
						texPathA = fallback;
				}

				OutputDebugStringA(ToDbgLine("[Veg] " + v.name + " diffuse: " + texPathA).c_str());
				if (!FileExistsA(texPathA))
				{
					OutputDebugStringA(ToDbgLine("[Veg] Texture not found for " + v.name).c_str());
					return false;
				}

				std::wstring texPathW(texPathA.begin(), texPathA.end());
				if (!INANOA::Assets::CreateTextureFromFileWIC(m_device, uploadCmd, texPathW, v.tex, v.texUpload))
				{
					OutputDebugStringA(ToDbgLine("[Veg] CreateTextureFromFileWIC failed for " + v.name).c_str());
					return false;
				}

				// SRV slotBase+0 = texture (t0 when bound as table base)
				{
					const UINT slot = v.heapSlotBase + 0;
					D3D12_CPU_DESCRIPTOR_HANDLE cpu = CpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);
					D3D12_GPU_DESCRIPTOR_HANDLE gpu = GpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);

					D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
					srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srv.Texture2D.MipLevels = 1;

					m_device->CreateShaderResourceView(v.tex.Get(), &srv, cpu);
					v.texSrvGpu = gpu;
				}
			}

			return true;
		}

		bool RendererBase::initVegetationFieldFromPoisson(ID3D12GraphicsCommandList* uploadCmd, size_t typeIndex, const std::string& ss2Path, float y, float scale)
		{
			if (!m_device || !uploadCmd)
				return false;
			if (typeIndex >= m_veg.size())
				return false;

			VegetationType& v = m_veg[typeIndex];
			if (!v.vb || !v.ib || v.indexCount == 0)
				return false;

			using namespace INANOA::SCENE::EXPERIMENTAL;

			SpatialSample* sample = SpatialSample::importBinaryFile(ss2Path.c_str());
			if (!sample)
			{
				OutputDebugStringA(ToDbgLine("[Veg] SpatialSample::importBinaryFile failed: " + v.name).c_str());
				return false;
			}

			const int num = sample->numSample();
			if (num <= 0)
			{
				delete sample;
				OutputDebugStringA(ToDbgLine("[Veg] SpatialSample has 0 samples: " + v.name).c_str());
				return false;
			}

			std::vector<GrassInstance> instances;
			instances.reserve(static_cast<size_t>(num));

			for (int i = 0; i < num; ++i)
			{
				const float* p = sample->position(i);
				if (!p)
					continue;

				const glm::vec3 pos(p[0], p[1] + y, p[2]);

				glm::mat4 model(1.0f);
				model = glm::translate(model, pos);
				model = glm::scale(model, glm::vec3(scale));

				GrassInstance gi{};
				memcpy(gi.m, glm::value_ptr(model), sizeof(gi.m));
				instances.push_back(gi);
			}

			delete sample;

			v.instanceCount = static_cast<UINT>(instances.size());
			if (v.instanceCount == 0)
				return false;

			const UINT instanceBytes = static_cast<UINT>(instances.size() * sizeof(GrassInstance));

			// Candidate instance buffer
			if (FAILED(m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(instanceBytes),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&v.instanceBuffer))))
				return false;

			if (FAILED(m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(instanceBytes),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&v.instanceUpload))))
				return false;

			void* mapped = nullptr;
			if (FAILED(v.instanceUpload->Map(0, nullptr, &mapped)))
				return false;

			memcpy(mapped, instances.data(), instanceBytes);
			v.instanceUpload->Unmap(0, nullptr);

			uploadCmd->CopyBufferRegion(v.instanceBuffer.Get(), 0, v.instanceUpload.Get(), 0, instanceBytes);
			uploadCmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				v.instanceBuffer.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

			// SRV slotBase+1 = candidate instances (StructuredBuffer)
			{
				const UINT slot = v.heapSlotBase + 1;
				D3D12_CPU_DESCRIPTOR_HANDLE cpu = CpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);
				D3D12_GPU_DESCRIPTOR_HANDLE gpu = GpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);

				D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
				srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srv.Format = DXGI_FORMAT_UNKNOWN;
				srv.Buffer.FirstElement = 0;
				srv.Buffer.NumElements = v.instanceCount;
				srv.Buffer.StructureByteStride = sizeof(GrassInstance);
				srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				m_device->CreateShaderResourceView(v.instanceBuffer.Get(), &srv, cpu);
				v.instanceSrvGpu = gpu;
			}

			// CPU fallback indirect args (single instanced draw)
			{
				const UINT argsBytes = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

				if (FAILED(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(argsBytes),
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&v.indirectArgs))))
					return false;

				if (FAILED(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(argsBytes),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&v.indirectArgsUpload))))
					return false;

				D3D12_DRAW_INDEXED_ARGUMENTS arg = {};
				arg.IndexCountPerInstance = v.indexCount;
				arg.InstanceCount = v.instanceCount;
				arg.StartIndexLocation = 0;
				arg.BaseVertexLocation = 0;
				arg.StartInstanceLocation = 0;

				void* mappedArgs = nullptr;
				if (FAILED(v.indirectArgsUpload->Map(0, nullptr, &mappedArgs)))
					return false;

				memcpy(mappedArgs, &arg, sizeof(arg));
				v.indirectArgsUpload->Unmap(0, nullptr);

				uploadCmd->CopyBufferRegion(v.indirectArgs.Get(), 0, v.indirectArgsUpload.Get(), 0, argsBytes);
				uploadCmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
					v.indirectArgs.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));
			}

			// Compute-cull resources per type
			{
				// Visible instances buffer: UAV+SRV
				if (FAILED(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(instanceBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
					nullptr,
					IID_PPV_ARGS(&v.visibleInstances))))
					return false;

				// Visible count (uint[1]) UAV
				if (FAILED(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					nullptr,
					IID_PPV_ARGS(&v.visibleCount))))
					return false;

				// Upload reset buffer (uint = 0)
				if (FAILED(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT)),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&v.visibleCountUpload))))
					return false;

				{
					UINT* p = nullptr;
					v.visibleCountUpload->Map(0, nullptr, reinterpret_cast<void**>(&p));
					*p = 0;
					v.visibleCountUpload->Unmap(0, nullptr);
				}

				// Indirect args written by CS (UAV) and consumed by ExecuteIndirect
				if (FAILED(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
					nullptr,
					IID_PPV_ARGS(&v.indirectArgsCS))))
					return false;

				// Descriptors:
				// slotBase+2: visible SRV
				// slotBase+3: visible UAV
				// slotBase+4: visibleCount UAV
				// slotBase+5: indirectArgs UAV
				{
					// slotBase+2 SRV (StructuredBuffer<GrassInstance>)
					const UINT slot = v.heapSlotBase + 2;
					D3D12_CPU_DESCRIPTOR_HANDLE cpu = CpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);
					D3D12_GPU_DESCRIPTOR_HANDLE gpu = GpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);

					D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
					desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
					desc.Format = DXGI_FORMAT_UNKNOWN;
					desc.Buffer.FirstElement = 0;
					desc.Buffer.NumElements = v.instanceCount;
					desc.Buffer.StructureByteStride = sizeof(GrassInstance);
					desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

					m_device->CreateShaderResourceView(v.visibleInstances.Get(), &desc, cpu);
					v.visibleSrvGpu = gpu;
				}

				{
					// slotBase+3 UAV (RWStructuredBuffer<GrassInstance>)
					const UINT slot = v.heapSlotBase + 3;
					D3D12_CPU_DESCRIPTOR_HANDLE cpu = CpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);
					D3D12_GPU_DESCRIPTOR_HANDLE gpu = GpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);

					D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
					desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
					desc.Format = DXGI_FORMAT_UNKNOWN;
					desc.Buffer.FirstElement = 0;
					desc.Buffer.NumElements = v.instanceCount;
					desc.Buffer.StructureByteStride = sizeof(GrassInstance);
					desc.Buffer.CounterOffsetInBytes = 0;
					desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

					m_device->CreateUnorderedAccessView(v.visibleInstances.Get(), nullptr, &desc, cpu);
					v.visibleUavGpu = gpu;
				}

				{
					// slotBase+4 UAV (uint count[1])
					const UINT slot = v.heapSlotBase + 4;
					D3D12_CPU_DESCRIPTOR_HANDLE cpu = CpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);
					D3D12_GPU_DESCRIPTOR_HANDLE gpu = GpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);

					D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
					desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
					desc.Format = DXGI_FORMAT_R32_UINT;
					desc.Buffer.FirstElement = 0;
					desc.Buffer.NumElements = 1;
					desc.Buffer.StructureByteStride = 0;
					desc.Buffer.CounterOffsetInBytes = 0;
					desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

					m_device->CreateUnorderedAccessView(v.visibleCount.Get(), nullptr, &desc, cpu);
					v.visibleCountUavGpu = gpu;
				}

				{
					// slotBase+5 UAV (RWByteAddressBuffer) for indirect args
					const UINT slot = v.heapSlotBase + 5;
					D3D12_CPU_DESCRIPTOR_HANDLE cpu = CpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);
					D3D12_GPU_DESCRIPTOR_HANDLE gpu = GpuHandleAt(m_srvHeap.Get(), m_srvInc, slot);

					D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
					desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
					desc.Format = DXGI_FORMAT_R32_TYPELESS;
					desc.Buffer.FirstElement = 0;
					desc.Buffer.NumElements = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) / 4;
					desc.Buffer.StructureByteStride = 0;
					desc.Buffer.CounterOffsetInBytes = 0;
					desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

					m_device->CreateUnorderedAccessView(v.indirectArgsCS.Get(), nullptr, &desc, cpu);
					v.indirectArgsCsvGpu = gpu;
				}

				v.computeReady = true;
			}

			return true;
		}

		void RendererBase::cullVegetationToViewFrustumCS(ID3D12GraphicsCommandList* commandList, size_t typeIndex, const glm::mat4& viewProj)
		{
			if (!commandList || !m_rootSignatureComputeGrass || !m_psoComputeGrassCull || !m_psoComputeGrassFinalize)
				return;
			if (typeIndex >= m_veg.size())
				return;

			VegetationType& v = m_veg[typeIndex];

			if (!v.computeReady || !v.instanceBuffer || v.instanceCount == 0 || !v.visibleInstances || !v.visibleCount || !v.indirectArgsCS)
				return;

			// Reset visibleCount = 0
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				v.visibleCount.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_COPY_DEST));

			commandList->CopyBufferRegion(v.visibleCount.Get(), 0, v.visibleCountUpload.Get(), 0, sizeof(UINT));

			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				v.visibleCount.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

			// Visible instances must be UAV during cull
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				v.visibleInstances.Get(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

			// Indirect args must be UAV during finalize write
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				v.indirectArgsCS.Get(),
				D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

			ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
			commandList->SetDescriptorHeaps(1, heaps);

			commandList->SetComputeRootSignature(m_rootSignatureComputeGrass.Get());

			struct CullCB
			{
				glm::mat4 viewProj;
				UINT candidateCount;
				UINT indexCountPerInstance;
				UINT pad0;
				UINT pad1;
			} cb;

			cb.viewProj = viewProj;
			cb.candidateCount = v.instanceCount;
			cb.indexCountPerInstance = v.indexCount;
			cb.pad0 = 0;
			cb.pad1 = 0;

			// Cull pass
			commandList->SetPipelineState(m_psoComputeGrassCull.Get());
			commandList->SetComputeRoot32BitConstants(0, 20, &cb, 0);

			commandList->SetComputeRootDescriptorTable(1, v.instanceSrvGpu);
			commandList->SetComputeRootDescriptorTable(2, v.visibleUavGpu);
			commandList->SetComputeRootDescriptorTable(3, v.visibleCountUavGpu);
			commandList->SetComputeRootDescriptorTable(4, v.indirectArgsCsvGpu);

			const UINT threads = 256;
			const UINT groups = (v.instanceCount + threads - 1) / threads;
			commandList->Dispatch(groups, 1, 1);

			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(nullptr));

			// Finalize pass
			commandList->SetPipelineState(m_psoComputeGrassFinalize.Get());
			commandList->SetComputeRoot32BitConstants(0, 20, &cb, 0);
			commandList->Dispatch(1, 1, 1);

			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(nullptr));

			// Back to SRV/INDIRECT for draw
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				v.visibleInstances.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				v.indirectArgsCS.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));
		}

		void RendererBase::drawVegetationIndirect(ID3D12GraphicsCommandList* commandList, size_t typeIndex)
		{
			if (!commandList || !m_cmdSigDrawIndexed || typeIndex >= m_veg.size())
				return;

			VegetationType& v = m_veg[typeIndex];
			if (!v.vb || !v.ib || v.indexCount == 0 || v.instanceCount == 0)
				return;

			ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
			commandList->SetDescriptorHeaps(1, heaps);

			commandList->SetGraphicsRootSignature(m_rootSignatureTextured.Get());
			commandList->SetPipelineState(m_pipelineStateTextured.Get());

			commandList->SetGraphicsRoot32BitConstants(0, 16, glm::value_ptr(m_viewMat), 0);
			commandList->SetGraphicsRoot32BitConstants(1, 16, glm::value_ptr(m_projMat), 0);

			glm::mat4 id(1.0f);
			commandList->SetGraphicsRoot32BitConstants(2, 16, glm::value_ptr(id), 0);

			commandList->SetGraphicsRootDescriptorTable(3, v.texSrvGpu);

			commandList->IASetVertexBuffers(0, 1, &v.vbv);
			commandList->IASetIndexBuffer(&v.ibv);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			const D3D12_GPU_DESCRIPTOR_HANDLE instSrv = (v.computeReady ? v.visibleSrvGpu : v.instanceSrvGpu);
			ID3D12Resource* args = (v.computeReady ? v.indirectArgsCS.Get() : v.indirectArgs.Get());
			if (!args)
				return;

			commandList->SetGraphicsRootDescriptorTable(4, instSrv);

			commandList->ExecuteIndirect(
				m_cmdSigDrawIndexed.Get(),
				1,
				args,
				0,
				nullptr,
				0);
		}

		// unchanged methods below...

		void RendererBase::setCamera(ID3D12GraphicsCommandList* commandList, const glm::mat4& projMat, const glm::mat4& viewMat, const glm::vec3& viewOrg) {
			this->m_projMat = projMat;
			this->m_viewMat = viewMat;
			this->m_viewPosition = glm::vec4(viewOrg, 1.0f);

			commandList->SetGraphicsRootSignature(m_rootSignature.Get());
			commandList->SetGraphicsRoot32BitConstants(0, 16, glm::value_ptr(this->m_viewMat), 0);
			commandList->SetGraphicsRoot32BitConstants(1, 16, glm::value_ptr(this->m_projMat), 0);
		}

		void RendererBase::resize(const int w, const int h) {
			this->m_frameWidth = w;
			this->m_frameHeight = h;
		}

		void RendererBase::clearRenderTarget(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) {
			static const float color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			commandList->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		}

		void RendererBase::useLinePSO(ID3D12GraphicsCommandList* commandList, bool useLines)
		{
			if (useLines)
			{
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