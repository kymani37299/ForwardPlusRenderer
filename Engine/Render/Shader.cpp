#include "Shader.h"

#include "Common.h"

#include <set>
#include <fstream>
#include <d3dcompiler.h>

#include "Render/Device.h"
#include "Utility/StringUtility.h"
#include "Utility/PathUtility.h"
#include "Utility/Hash.h"

namespace GFX
{
	namespace // Util
	{
		DXGI_FORMAT ToDXGIFormat(D3D12_SIGNATURE_PARAMETER_DESC paramDesc)
		{
			if (paramDesc.Mask == 1)
			{
				switch (paramDesc.ComponentType)
				{
				case D3D_REGISTER_COMPONENT_UINT32:
					return DXGI_FORMAT_R32_UINT;
				case D3D_REGISTER_COMPONENT_SINT32:
					return DXGI_FORMAT_R32_SINT;
				case D3D_REGISTER_COMPONENT_FLOAT32:
					return DXGI_FORMAT_R32_FLOAT;
				}
			}
			else if (paramDesc.Mask <= 3)
			{
				switch (paramDesc.ComponentType)
				{
				case D3D_REGISTER_COMPONENT_UINT32:
					return DXGI_FORMAT_R32G32_UINT;
				case D3D_REGISTER_COMPONENT_SINT32:
					return DXGI_FORMAT_R32G32_SINT;
				case D3D_REGISTER_COMPONENT_FLOAT32:
					return DXGI_FORMAT_R32G32_FLOAT;
				}
			}
			else if (paramDesc.Mask <= 7)
			{
				switch (paramDesc.ComponentType)
				{
				case D3D_REGISTER_COMPONENT_UINT32:
					return DXGI_FORMAT_R32G32B32_UINT;
				case D3D_REGISTER_COMPONENT_SINT32:
					return DXGI_FORMAT_R32G32B32_SINT;
				case D3D_REGISTER_COMPONENT_FLOAT32:
					return DXGI_FORMAT_R32G32B32_FLOAT;
				}
			}
			else if (paramDesc.Mask <= 15)
			{
				switch (paramDesc.ComponentType)
				{
				case D3D_REGISTER_COMPONENT_UINT32:
					return DXGI_FORMAT_R32G32B32A32_UINT;
				case D3D_REGISTER_COMPONENT_SINT32:
					return DXGI_FORMAT_R32G32B32A32_SINT;
				case D3D_REGISTER_COMPONENT_FLOAT32:
					return DXGI_FORMAT_R32G32B32A32_FLOAT;
				}
			}

			NOT_IMPLEMENTED;
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	namespace // Shader compiler
	{
		bool ReadFile(const std::string& path, std::vector<std::string>& content)
		{
			content.clear();

			std::ifstream fileStream(path, std::ios::in);

			if (!fileStream.is_open()) {
				return false;
			}

			std::string line = "";
			while (!fileStream.eof()) {
				std::getline(fileStream, line);
				content.push_back(line);
			}

			fileStream.close();
			return true;
		}

		void ReadShaderFile(std::string path, std::string& shaderCode)
		{
			shaderCode = "";

			std::string rootPath = PathUtility::GetPathWitoutFile(path);
			std::vector<std::string> shaderContent;
			std::vector<std::string> tmp;

			bool readSuccess = false;

			readSuccess = ReadFile(path, shaderContent);
			ASSERT(readSuccess, "Failed to load shader: " + path);

			shaderContent.insert((shaderContent.begin()), tmp.begin(), tmp.end());

			std::set<std::string> loadedFiles = {};
			for (size_t i = 0; i < shaderContent.size(); i++)
			{
				std::string& line = shaderContent[i];
				if (StringUtility::Contains(line, "#include"))
				{
					std::string fileName = line;
					StringUtility::ReplaceAll(fileName, "#include", "");
					StringUtility::ReplaceAll(fileName, " ", "");
					StringUtility::ReplaceAll(fileName, "\"", "");

					if (loadedFiles.count(fileName)) continue;
					loadedFiles.insert(fileName);

					readSuccess = ReadFile(rootPath + fileName, tmp);
					ASSERT(readSuccess, "Failed to include file in shader: " + rootPath + fileName);
					shaderContent.insert((shaderContent.begin() + (i + 1)), tmp.begin(), tmp.end());
				}
				else
				{
					shaderCode.append(line + "\n");
				}
			}
		}

		D3D_SHADER_MACRO* CompileConfiguration(const std::vector<std::string>& configuration)
		{
			if (configuration.size() == 0) return nullptr;

			const size_t numConfigs = configuration.size();
			D3D_SHADER_MACRO* compiledConfig = (D3D_SHADER_MACRO*)malloc(sizeof(D3D_SHADER_MACRO) * (numConfigs + 1));
			for (size_t i = 0; i < numConfigs; i++)
			{
				compiledConfig[i].Name = configuration[i].c_str();
				compiledConfig[i].Definition = "";
			}
			compiledConfig[numConfigs].Name = NULL;
			compiledConfig[numConfigs].Definition = NULL;

			return compiledConfig;
		}

		ID3DBlob* ReadBlobFromFile(const std::string& path, const std::string& shaderCode, const std::string& entry, const std::string& hlsl_target, D3D_SHADER_MACRO* configuration)
		{
			ID3DBlob* shaderCompileErrorsBlob, * blob;
			HRESULT hResult = D3DCompile(shaderCode.c_str(), shaderCode.size(), nullptr, configuration, nullptr, entry.c_str(), hlsl_target.c_str(), 0, 0, &blob, &shaderCompileErrorsBlob);
			if (FAILED(hResult))
			{
				const char* errorString = NULL;
				if (shaderCompileErrorsBlob) {
					errorString = (const char*)shaderCompileErrorsBlob->GetBufferPointer();
					shaderCompileErrorsBlob->Release();
				}
				std::cout << "Shader compiler error for file: " << path << "\nError message:" << errorString << std::endl;
				return nullptr;
			}
			return blob;
		}

		std::vector<D3D12_INPUT_ELEMENT_DESC> CreateInputLayout(ID3DBlob* vsBlob, bool multiInput)
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

			ID3D12ShaderReflection* reflection;
			API_CALL(D3DReflect(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflection));
			D3D12_SHADER_DESC desc;
			reflection->GetDesc(&desc);

			bool lastPerInstance = false; // Last input was perInstance

			inputLayout.resize(desc.InputParameters);

			for (size_t i = 0; i < desc.InputParameters; i++)
			{
				D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
				reflection->GetInputParameterDesc(i, &paramDesc);

				const bool perInstance = std::string(paramDesc.SemanticName).find("I_") == 0 ||
					std::string(paramDesc.SemanticName).find("i_") == 0;

				// If we have mixed inputs don't create single slot input layout
				if (i > 0 && !multiInput && perInstance != lastPerInstance)
				{
					reflection->Release();
					return {};
				}

				inputLayout[i].SemanticName = paramDesc.SemanticName;
				inputLayout[i].SemanticIndex = paramDesc.SemanticIndex;
				inputLayout[i].Format = ToDXGIFormat(paramDesc);
				inputLayout[i].InputSlot = multiInput ? i : 0;
				inputLayout[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
				inputLayout[i].InputSlotClass = perInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
				inputLayout[i].InstanceDataStepRate = perInstance ? 1 : 0;

				lastPerInstance = perInstance;
			}

			return inputLayout;
		}

		// Returns true if compile success
		bool CompileShader(const std::string path, const uint32_t creationFlags, const std::vector<std::string>& defines, CompiledShader& compiledShader)
		{
			// TODO: Port to 6_0
			static const std::string SHADER_VERSION = "5_0";

			std::string shaderCode;
			ReadShaderFile(path, shaderCode);

			D3D_SHADER_MACRO* configuration = CompileConfiguration(defines);
			
			compiledShader.Data.resize(6);
			compiledShader.Data[0] = creationFlags & VS ? ReadBlobFromFile(path, shaderCode, "VS", "vs_" + SHADER_VERSION, configuration) : nullptr;
			compiledShader.Data[1] = creationFlags & GS ? ReadBlobFromFile(path, shaderCode, "GS", "gs_" + SHADER_VERSION, configuration) : nullptr;
			compiledShader.Data[2] = creationFlags & HS ? ReadBlobFromFile(path, shaderCode, "HS", "hs_" + SHADER_VERSION, configuration) : nullptr;
			compiledShader.Data[3] = creationFlags & DS ? ReadBlobFromFile(path, shaderCode, "DS", "ds_" + SHADER_VERSION, configuration) : nullptr;
			compiledShader.Data[4] = creationFlags & PS ? ReadBlobFromFile(path, shaderCode, "PS", "ps_" + SHADER_VERSION, configuration) : nullptr;
			compiledShader.Data[5] = creationFlags & CS ? ReadBlobFromFile(path, shaderCode, "CS", "cs_" + SHADER_VERSION, configuration) : nullptr;

			compiledShader.Vertex = compiledShader.Data[0].Get() ? D3D12_SHADER_BYTECODE{ compiledShader.Data[0]->GetBufferPointer(), compiledShader.Data[0]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };
			compiledShader.Geometry = compiledShader.Data[1].Get() ? D3D12_SHADER_BYTECODE{ compiledShader.Data[1]->GetBufferPointer(), compiledShader.Data[1]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };
			compiledShader.Hull = compiledShader.Data[2].Get() ? D3D12_SHADER_BYTECODE{ compiledShader.Data[2]->GetBufferPointer(), compiledShader.Data[2]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };
			compiledShader.Domain = compiledShader.Data[3].Get() ? D3D12_SHADER_BYTECODE{ compiledShader.Data[3]->GetBufferPointer(), compiledShader.Data[3]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };
			compiledShader.Pixel = compiledShader.Data[4].Get() ? D3D12_SHADER_BYTECODE{ compiledShader.Data[4]->GetBufferPointer(), compiledShader.Data[4]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };
			compiledShader.Compute = compiledShader.Data[5].Get() ? D3D12_SHADER_BYTECODE{ compiledShader.Data[5]->GetBufferPointer(), compiledShader.Data[5]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };

			if (compiledShader.Vertex.BytecodeLength)
			{
				compiledShader.InputLayout = CreateInputLayout(compiledShader.Data[0].Get(), false);
				compiledShader.InputLayoutMultiInput = CreateInputLayout(compiledShader.Data[0].Get(), true);
			}

			return true;
		}
	}

	static ShaderHash GetImlementationHash(const std::vector<std::string>& defines, uint32_t shaderStages)
	{
		// TODO: Sort defines so we don't make duplicates
		std::string defAll = " ";
		for (const std::string& def : defines) defAll += def;
		if (shaderStages & VS) defAll += "VS";
		if (shaderStages & PS) defAll += "PS";
		if (shaderStages & CS) defAll += "CS";
		return Hash::Crc32(defAll);
	}

	const CompiledShader& GetCompiledShader(Shader* shader, const std::vector<std::string>& defines, uint32_t shaderStages)
	{
		const uint32_t implHash = GetImlementationHash(defines, shaderStages);
		if (!shader->Implementations.contains(implHash))
		{
			bool success = CompileShader(shader->Path, shaderStages, defines, shader->Implementations[implHash]);
			ASSERT(success, "Shader compilation failed!");
		}
		return shader->Implementations.at(implHash);
	}

	void ReloadAllShaders()
	{
		for (Shader* shader : Shader::AllShaders)
		{
			shader->Implementations.clear();
		}
	}

}

std::set<Shader*> Shader::AllShaders;
