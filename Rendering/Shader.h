#pragma once

#include <string>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace INANOA {
	namespace D3D12 { // Changed namespace to D3D12

		// Replacement for GL enums
		enum class ShaderType {
			VERTEX,
			PIXEL, // Equivalent to GL_FRAGMENT_SHADER
			COMPUTE,
			UNKNOWN
		};

		enum class ShaderStatus {
			NULL_SHADER,
			NULL_SHADER_CODE,
			READY,
			COMPILE_ERROR
		};

		enum class ShaderProgramStatus {
			NULL_VERTEX_SHADER_FRAGMENT_SHADER,
			NULL_VERTEX_SHADER,
			NULL_FRAGMENT_SHADER,
			PROGRAM_ID_READY, // Indicates Blobs are present
			READY,
			LINK_ERROR // Won't happen in D3D partial context, but kept for API parity
		};

		class Shader {
		public:
			explicit Shader(const ShaderType shaderType);
			virtual ~Shader();

			// Reads text from file, then calls appendShaderCode -> compileShader
			bool createShaderFromFile(const std::string& fileFullpath);

			void appendShaderCode(const std::string& code);

			// Uses D3DCompile to build the DXBC/DXIL
			bool compileShader();

			void releaseShader();

			inline std::string shaderInfoLog() const { return m_shaderInfoLog; }
			inline ShaderStatus status() const { return m_shaderStatus; }
			inline ShaderType shaderType() const { return m_shaderType; }

			// D3D12 Specific: Get the compiled bytecode
			inline ID3DBlob* getBlob() const { return m_blob.Get(); }

		private:
			const ShaderType m_shaderType;

			Microsoft::WRL::ComPtr<ID3DBlob> m_blob; // Replaces m_shaderId

			std::string m_shaderInfoLog;
			std::string m_shaderCode;
			ShaderStatus m_shaderStatus;
		};

		class ShaderProgram {
		public:
			explicit ShaderProgram();
			virtual ~ShaderProgram();

			bool init(); // Resets state
			bool attachShader(const Shader* shader);
			ShaderProgramStatus checkStatus();

			// In D3D12, "linking" doesn't strictly happen here, but we can finalize validation
			void linkProgram();

			// Helper to clean up API usage, though D3D PSOs are immutable
			void useProgram() {}

			inline ShaderProgramStatus status() const { return m_shaderProgramStatus; }

			// Static creators
			static ShaderProgram* createShaderProgram(const std::string& vsResource, const std::string& fsResource);
			static ShaderProgram* createShaderProgramForComputeShader(const std::string& csResource);

			// D3D12 Specific Accessors used by RendererBase to create PSOs
			inline ID3DBlob* getVS() const { return m_vsBlob.Get(); }
			inline ID3DBlob* getPS() const { return m_psBlob.Get(); }
			inline ID3DBlob* getCS() const { return m_csBlob.Get(); }

		private:
			// In D3D12, we hold the blobs until the PSO is created by the Renderer
			Microsoft::WRL::ComPtr<ID3DBlob> m_vsBlob;
			Microsoft::WRL::ComPtr<ID3DBlob> m_psBlob;
			Microsoft::WRL::ComPtr<ID3DBlob> m_csBlob;

			bool m_vsReady = false;
			bool m_fsReady = false;
			bool m_csReady = false;
			ShaderProgramStatus m_shaderProgramStatus;
		};
	}
}