#include "Shader.h"

#include "Common.h"

#include <dxc/hlsl/DxilContainer.h>

#include <set>
#include <fstream>
#include <d3d12shader.h>

#include "Render/Device.h"
#include "Utility/StringUtility.h"
#include "Utility/PathUtility.h"
#include "Utility/Hash.h"

namespace GFX
{
	namespace ShaderCompiler
	{
		struct DXCCompiler
		{
			ComPtr<IDxcLibrary> Library;
			ComPtr<IDxcCompiler> Compiler;
			ComPtr<IDxcIncludeHandler> IncludeHandler;
		};

		DXCCompiler Compiler;

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

		IDxcOperationResult* DXC_Compile(const std::wstring& path, const std::wstring& entryPoint, const std::wstring& targetProfile, const std::vector<DxcDefine>& defines)
		{
			ComPtr<IDxcBlobEncoding> sourceBlob;
			uint32_t codePage = CP_UTF8;
			API_CALL(Compiler.Library->CreateBlobFromFile(path.c_str(), &codePage, sourceBlob.GetAddressOf()));

			IDxcOperationResult* result;

			HRESULT hr = Compiler.Compiler->Compile(
				sourceBlob.Get(), path.c_str(),
				entryPoint.c_str(), targetProfile.c_str(),
				nullptr, 0,
				defines.empty() ? nullptr : defines.data(), (UINT) defines.size(),
				Compiler.IncludeHandler.Get(),
				&result);

			ASSERT(SUCCEEDED(hr), "Shader compilation failed!");
			
			result->GetStatus(&hr);
			
			if(FAILED(hr))
			{
				if (result)
				{
					ComPtr<IDxcBlobEncoding> errorsBlob;
					hr = result->GetErrorBuffer(&errorsBlob);
					if (SUCCEEDED(hr) && errorsBlob)
					{
						const char* errorString = (const char*)errorsBlob->GetBufferPointer();
						std::cout << "Compilation failed with errors:" << errorString << std::endl;
					}
				}
				ASSERT(0, "Shader compilation failed!");
			}

			return result;
		}

		std::vector<D3D12_INPUT_ELEMENT_DESC> DXC_CreateInputLayout(IDxcBlob* vsBlob, bool multiInput)
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout{};

			ID3D12ShaderReflection* reflection;
			ComPtr<IDxcContainerReflection> dxcReflection;
			UINT32 shaderIdx;
			API_CALL(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(dxcReflection.GetAddressOf())));
			API_CALL(dxcReflection->Load(vsBlob));
			API_CALL(dxcReflection->FindFirstPartKind(hlsl::DFCC_DXIL, &shaderIdx));
			API_CALL(dxcReflection->GetPartReflection(shaderIdx, __uuidof(ID3D12ShaderReflection), (void**)&reflection));

			D3D12_SHADER_DESC desc{};
			reflection->GetDesc(&desc);

			bool lastPerInstance = false;
			uint32_t multiSlotInputSlot = 0;
			for (UINT i = 0; i < desc.InputParameters; i++)
			{
				D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
				reflection->GetInputParameterDesc(i, &paramDesc);

				if (paramDesc.SystemValueType != D3D_NAME_UNDEFINED) continue;

				const bool perInstance = std::string(paramDesc.SemanticName).find("I_") == 0 ||
					std::string(paramDesc.SemanticName).find("i_") == 0;

				// If we have mixed inputs don't create single slot input layout
				if (i > 0 && !multiInput && perInstance != lastPerInstance)
				{
					reflection->Release();
					return {};
				}

				D3D12_INPUT_ELEMENT_DESC inputElement{};
				inputElement.SemanticName = paramDesc.SemanticName;
				inputElement.SemanticIndex = paramDesc.SemanticIndex;
				inputElement.Format = ToDXGIFormat(paramDesc);
				inputElement.InputSlot = multiInput ? multiSlotInputSlot++ : 0;
				inputElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
				inputElement.InputSlotClass = perInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
				inputElement.InstanceDataStepRate = perInstance ? 1 : 0;
				inputLayout.push_back(inputElement);

				lastPerInstance = perInstance;
			}

			return inputLayout;
		}

		// Returns true if compile success
		bool CompileShader(const std::string path, const uint32_t creationFlags, const std::vector<std::string>& defines, CompiledShader& compiledShader)
		{
			static const std::wstring SHADER_VERSION = L"6_0";

			std::vector<std::wstring> dxcDefinesW;
			std::vector<DxcDefine> dxcDefines;
			dxcDefines.resize(defines.size());
			dxcDefinesW.resize(defines.size());

			for (uint32_t i = 0; i < defines.size(); i++)
			{
				dxcDefinesW[i] = StringUtility::ToWideString(defines[i]);
				dxcDefines[i].Name = dxcDefinesW[i].c_str();
				dxcDefines[i].Value = 0;
			}

			const std::wstring wPath = StringUtility::ToWideString(path);

			compiledShader.Data.resize(6);
			compiledShader.Data[0] = creationFlags & VS ? DXC_Compile(wPath, L"VS", L"vs_" + SHADER_VERSION, dxcDefines) : nullptr;
			compiledShader.Data[1] = creationFlags & GS ? DXC_Compile(wPath, L"GS", L"gs_" + SHADER_VERSION, dxcDefines) : nullptr;
			compiledShader.Data[2] = creationFlags & HS ? DXC_Compile(wPath, L"HS", L"hs_" + SHADER_VERSION, dxcDefines) : nullptr;
			compiledShader.Data[3] = creationFlags & DS ? DXC_Compile(wPath, L"DS", L"ds_" + SHADER_VERSION, dxcDefines) : nullptr;
			compiledShader.Data[4] = creationFlags & PS ? DXC_Compile(wPath, L"PS", L"ps_" + SHADER_VERSION, dxcDefines) : nullptr;
			compiledShader.Data[5] = creationFlags & CS ? DXC_Compile(wPath, L"CS", L"cs_" + SHADER_VERSION, dxcDefines) : nullptr;

			ComPtr<IDxcBlob> shaderBlobs[6];
			for (uint32_t i = 0; i < 6; i++)
			{
				if (compiledShader.Data[i]) API_CALL(compiledShader.Data[i]->GetResult(shaderBlobs[i].GetAddressOf()));
			}

			compiledShader.Vertex = shaderBlobs[0].Get() ? D3D12_SHADER_BYTECODE{ shaderBlobs[0]->GetBufferPointer(), shaderBlobs[0]->GetBufferSize()} : D3D12_SHADER_BYTECODE{nullptr, 0};
			compiledShader.Geometry = shaderBlobs[1].Get() ? D3D12_SHADER_BYTECODE{ shaderBlobs[1]->GetBufferPointer(), shaderBlobs[1]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };
			compiledShader.Hull = shaderBlobs[2].Get() ? D3D12_SHADER_BYTECODE{ shaderBlobs[2]->GetBufferPointer(), shaderBlobs[2]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };
			compiledShader.Domain = shaderBlobs[3].Get() ? D3D12_SHADER_BYTECODE{ shaderBlobs[3]->GetBufferPointer(), shaderBlobs[3]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };
			compiledShader.Pixel = shaderBlobs[4].Get() ? D3D12_SHADER_BYTECODE{ shaderBlobs[4]->GetBufferPointer(), shaderBlobs[4]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };
			compiledShader.Compute = shaderBlobs[5].Get() ? D3D12_SHADER_BYTECODE{ shaderBlobs[5]->GetBufferPointer(), shaderBlobs[5]->GetBufferSize() } : D3D12_SHADER_BYTECODE{ nullptr, 0 };

			if (compiledShader.Vertex.BytecodeLength)
			{
				compiledShader.InputLayout = DXC_CreateInputLayout(shaderBlobs[0].Get(), false);
				compiledShader.InputLayoutMultiInput = DXC_CreateInputLayout(shaderBlobs[0].Get(), true);
			}

			return true;
		}
	}

	void InitShaderCompiler()
	{
		using namespace ShaderCompiler;

		API_CALL(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(Compiler.Library.GetAddressOf())));
		API_CALL(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(Compiler.Compiler.GetAddressOf())));
		API_CALL(Compiler.Library->CreateIncludeHandler(&Compiler.IncludeHandler));
	}

	void DestroyShaderCompiler()
	{
		using namespace ShaderCompiler;

		Compiler.Library = nullptr;
		Compiler.Compiler = nullptr;
		Compiler.IncludeHandler = nullptr;
	}

	static ShaderHash GetImlementationHash(const std::vector<std::string>& defines, uint32_t shaderStages)
	{
		// TODO: Sort defines so we don't make duplicates
		std::string defAll = " ";
		for (const std::string& def : defines) defAll += def;
		if (shaderStages & VS) defAll += "VS";
		if (shaderStages & HS) defAll += "HS";
		if (shaderStages & DS) defAll += "DS";
		if (shaderStages & GS) defAll += "GS";
		if (shaderStages & PS) defAll += "PS";
		if (shaderStages & CS) defAll += "CS";
		return Hash::Crc32(defAll);
	}

	const CompiledShader& GetCompiledShader(Shader* shader, const std::vector<std::string>& defines, uint32_t shaderStages)
	{
		const uint32_t implHash = GetImlementationHash(defines, shaderStages);
		if (!shader->Implementations.contains(implHash))
		{
			bool success = ShaderCompiler::CompileShader(shader->Path, shaderStages, defines, shader->Implementations[implHash]);
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
