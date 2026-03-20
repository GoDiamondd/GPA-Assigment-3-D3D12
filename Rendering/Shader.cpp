#include "stdafx.h"

#include "Shader.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

// Link against the D3D Compiler
#pragma comment(lib, "d3dcompiler.lib")

namespace INANOA {
	namespace D3D12 {

		Shader::Shader(const ShaderType shaderType) : m_shaderType(shaderType) {
			this->m_shaderStatus = ShaderStatus::NULL_SHADER_CODE;
		}

		Shader::~Shader() {
			this->releaseShader();
		}

		void Shader::appendShaderCode(const std::string& code) {
			this->m_shaderCode = code;
			// Reset status so we can compile again
			this->m_shaderStatus = ShaderStatus::NULL_SHADER;
		}

		bool Shader::compileShader() {
			// In OpenGL, NULL_SHADER just meant ID hadn't been created. 
			// In D3D, we check if we have code.
			if (this->m_shaderCode.empty()) {
				return false;
			}

			// Determine Target Profile (e.g., vs_5_0, ps_5_0)
			// We assume Shader Model 5.0 for C++14/D3D12 baseline.
			const char* targetProfile = nullptr;
			const char* entryPoint = "main"; // Default entry point, can be customized or passed in

			switch (m_shaderType) {
			case ShaderType::VERTEX:
				targetProfile = "vs_5_0";
				entryPoint = "VSMain"; // Common convention, or reuse "main"
				break;
			case ShaderType::PIXEL:
				targetProfile = "ps_5_0";
				entryPoint = "PSMain";
				break;
			case ShaderType::COMPUTE:
				targetProfile = "cs_5_0";
				entryPoint = "CSMain";
				break;
			default:
				this->m_shaderInfoLog = "Unknown shader type";
				return false;
			}

			UINT compileFlags = 0;
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

			Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
			std::cout << "Compiling Shader..." << std::endl;
			std::cout << "Entry Point: " << entryPoint << std::endl;
			std::cout << "Code Snippet: " << this->m_shaderCode.substr(0, 50) << "..." << std::endl;

			// D3DCompile compiles HLSL source code into bytecode
			HRESULT hr = D3DCompile(
				this->m_shaderCode.c_str(),
				this->m_shaderCode.length(),
				nullptr, // Source name
				nullptr, // Defines
				nullptr, // Includes
				entryPoint,
				targetProfile,
				compileFlags,
				0,
				&this->m_blob,
				&errorBlob
			);

			if (FAILED(hr)) {
				this->m_shaderStatus = ShaderStatus::COMPILE_ERROR;
				if (errorBlob) {
					this->m_shaderInfoLog = (char*)errorBlob->GetBufferPointer();
				}
				else {
					this->m_shaderInfoLog = "Unknown D3DCompile error";
				}
				return false;
			}

			// Compile success
			this->m_shaderInfoLog = "ready";
			this->m_shaderStatus = ShaderStatus::READY;

			return true;
		}

		void Shader::releaseShader() {
			m_blob.Reset();
			m_shaderStatus = ShaderStatus::NULL_SHADER;
		}

		bool Shader::createShaderFromFile(const std::string& fileFullpath) {
			std::ifstream inputStream;
			inputStream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

			std::string shaderCode;
			try {
				// Debug Log: Print what we are trying to open
				std::cout << "[Shader] Opening file: " << fileFullpath << " ... ";

				inputStream.open(fileFullpath);
				std::stringstream shaderCodeStream;
				shaderCodeStream << inputStream.rdbuf();
				inputStream.close();

				shaderCode = shaderCodeStream.str();
				std::cout << "OK (" << shaderCode.length() << " bytes)" << std::endl;
			}
			catch (std::ifstream::failure& e) {
				// Debug Log: Print failure
				std::cout << "FAILED!" << std::endl;
				std::cout << "[Shader] Error reading file: " << e.what() << std::endl;

				// Print Current Working Directory to help debug path issues
				char cwd[1024];
				if (GetCurrentDirectoryA(1024, cwd)) std::cout << "[Shader] Current Dir: " << cwd << std::endl;

				this->m_shaderInfoLog = e.what();
				this->m_shaderStatus = ShaderStatus::NULL_SHADER_CODE;
				return false;
			}

			this->appendShaderCode(shaderCode);

			// Attempt compile and print result
			bool result = this->compileShader();
			if (!result) {
				std::cout << "[Shader] Compilation Failed:\n" << this->m_shaderInfoLog << std::endl;
			}
			return result;
		}

		// ========================================================
		// SHADER PROGRAM
		// ========================================================

		ShaderProgram::ShaderProgram() {
			this->m_shaderProgramStatus = ShaderProgramStatus::NULL_VERTEX_SHADER_FRAGMENT_SHADER;
			this->m_vsReady = false;
			this->m_fsReady = false;
			this->m_csReady = false;
		}

		ShaderProgram::~ShaderProgram() {}

		bool ShaderProgram::init() {
			m_vsBlob.Reset();
			m_psBlob.Reset();
			m_csBlob.Reset();
			m_vsReady = false;
			m_fsReady = false;
			m_csReady = false;

			this->m_shaderProgramStatus = ShaderProgramStatus::PROGRAM_ID_READY;
			return true;
		}

		bool ShaderProgram::attachShader(const Shader* shader) {
			if (this->m_shaderProgramStatus == ShaderProgramStatus::LINK_ERROR) {
				return false;
			}
			if (shader->status() != ShaderStatus::READY) {
				return false;
			}

			// Copy the pointer to the bytecode blob
			// Note: In D3D12, we don't have a single "Program" object. 
			// We store the blobs here to eventually give to the Pipeline State Object.
			if (shader->shaderType() == ShaderType::VERTEX) {
				this->m_vsBlob = shader->getBlob();
				this->m_vsReady = true;
			}
			else if (shader->shaderType() == ShaderType::PIXEL) {
				this->m_psBlob = shader->getBlob();
				this->m_fsReady = true;
			}
			else if (shader->shaderType() == ShaderType::COMPUTE) {
				this->m_csBlob = shader->getBlob();
				this->m_csReady = true;
			}
			return true;
		}

		ShaderProgramStatus ShaderProgram::checkStatus() {
			if (this->m_csReady == true) {
				this->m_shaderProgramStatus = ShaderProgramStatus::READY;
			}
			else if (this->m_vsReady == false && this->m_fsReady == false) {
				this->m_shaderProgramStatus = ShaderProgramStatus::NULL_VERTEX_SHADER_FRAGMENT_SHADER;
			}
			else if (this->m_vsReady == false) {
				this->m_shaderProgramStatus = ShaderProgramStatus::NULL_VERTEX_SHADER;
			}
			else if (this->m_fsReady == false) {
				this->m_shaderProgramStatus = ShaderProgramStatus::NULL_FRAGMENT_SHADER;
			}
			else {
				this->m_shaderProgramStatus = ShaderProgramStatus::READY;
			}

			return this->m_shaderProgramStatus;
		}

		void ShaderProgram::linkProgram() {
			if (this->m_shaderProgramStatus != ShaderProgramStatus::READY) {
				return;
			}
			// In D3D12, compilation is disconnected from linking (PSO creation).
			// We consider the "Program" wrapper linked if we have the necessary blobs.
		}

		// ======================================
		// Static Creators
		// ======================================

		ShaderProgram* ShaderProgram::createShaderProgram(const std::string& vsResource, const std::string& fsResource) {
			Shader* vsShader = new Shader(ShaderType::VERTEX);
			// Note: vsResource is treated as file path
			vsShader->createShaderFromFile(vsResource);
			if (vsShader->status() != ShaderStatus::READY) {
				std::cout << "VS Error: " << vsShader->shaderInfoLog() << "\n";
				delete vsShader;
				return nullptr;
			}

			Shader* fsShader = new Shader(ShaderType::PIXEL);
			fsShader->createShaderFromFile(fsResource);
			if (fsShader->status() != ShaderStatus::READY) {
				std::cout << "PS Error: " << fsShader->shaderInfoLog() << "\n";
				delete vsShader;
				delete fsShader;
				return nullptr;
			}

			ShaderProgram* shaderProgram = new ShaderProgram();
			shaderProgram->init();
			shaderProgram->attachShader(vsShader);
			shaderProgram->attachShader(fsShader);

			shaderProgram->checkStatus();
			if (shaderProgram->status() != ShaderProgramStatus::READY) {
				delete vsShader;
				delete fsShader;
				delete shaderProgram;
				return nullptr;
			}
			shaderProgram->linkProgram();

			// Once attached (ComPtr copy), we can delete the individual shader wrappers
			// The blobs inside them are ref-counted by the ShaderProgram
			delete vsShader;
			delete fsShader;

			return shaderProgram;
		}

		ShaderProgram* ShaderProgram::createShaderProgramForComputeShader(const std::string& csResource) {
			Shader* csShader = new Shader(ShaderType::COMPUTE);
			csShader->createShaderFromFile(csResource);
			if (csShader->status() != ShaderStatus::READY) {
				std::cout << "CS Error: " << csShader->shaderInfoLog() << "\n";
				delete csShader;
				return nullptr;
			}

			ShaderProgram* shaderProgram = new ShaderProgram();
			shaderProgram->init();
			shaderProgram->attachShader(csShader);
			shaderProgram->checkStatus();

			if (shaderProgram->status() != ShaderProgramStatus::READY) {
				delete csShader;
				delete shaderProgram;
				return nullptr;
			}
			shaderProgram->linkProgram();

			delete csShader;
			return shaderProgram;
		}
	}
}