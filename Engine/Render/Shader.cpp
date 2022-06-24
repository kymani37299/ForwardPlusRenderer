#include "Shader.h"

#include <set>
#include <vector>
#include <fstream>
#include <d3dcompiler.h>

#include "Render/Device.h"
#include "Render/Resource.h"
#include "Utility/StringUtility.h"
#include "Utility/PathUtility.h"
#include "Utility/Hash.h"

namespace GFX
{
    namespace // Util
    {
        DXGI_FORMAT ToDXGIFormat(D3D11_SIGNATURE_PARAMETER_DESC paramDesc)
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
            static const std::string commonInclude = "gp/gfx/GPShaderCommon.h";

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

        ID3D11InputLayout* CreateInputLayout(ID3DBlob* vsBlob, bool multiInput)
        {
            ID3D11ShaderReflection* reflection;
            API_CALL(D3DReflect(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflection));
            D3D11_SHADER_DESC desc;
            reflection->GetDesc(&desc);

            bool lastPerInstance = false; // Last input was perInstance
            std::vector<D3D11_INPUT_ELEMENT_DESC> inputElements(desc.InputParameters);
            for (size_t i = 0; i < desc.InputParameters; i++)
            {
                D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
                reflection->GetInputParameterDesc(i, &paramDesc);

                const bool perInstance = std::string(paramDesc.SemanticName).find("I_") == 0 ||
                    std::string(paramDesc.SemanticName).find("i_") == 0;

                // If we have mixed inputs don't create single slot input layout
                if (i > 0 && !multiInput && perInstance != lastPerInstance)
                {
                    reflection->Release();
                    return nullptr;
                }

                inputElements[i].SemanticName = paramDesc.SemanticName;
                inputElements[i].SemanticIndex = paramDesc.SemanticIndex;
                inputElements[i].Format = ToDXGIFormat(paramDesc);
                inputElements[i].InputSlot = multiInput ? i : 0;
                inputElements[i].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
                inputElements[i].InputSlotClass = perInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
                inputElements[i].InstanceDataStepRate = perInstance ? 1 : 0;

                lastPerInstance = perInstance;
            }

            ID3D11InputLayout* inputLayout;
            API_CALL(Device::Get()->GetHandle()->CreateInputLayout(inputElements.data(), desc.InputParameters, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout));

            reflection->Release();

            return inputLayout;
        }

        // Returns true if compile success
		bool CompileShader(const std::string path, const uint32_t creationFlags, const std::vector<std::string>& defines, ShaderImplementation& implementation)
		{
            static const std::string SHADER_VERSION = "5_0";

			std::string shaderCode;
			ReadShaderFile(path, shaderCode);

			D3D_SHADER_MACRO* configuration = CompileConfiguration(defines);
			ID3DBlob* vsBlob = creationFlags & VS ? ReadBlobFromFile(path, shaderCode, "VS", "vs_" + SHADER_VERSION, configuration) : nullptr;
			ID3DBlob* psBlob = creationFlags & PS ? ReadBlobFromFile(path, shaderCode, "PS", "ps_" + SHADER_VERSION, configuration) : nullptr;
			ID3DBlob* csBlob = creationFlags & CS ? ReadBlobFromFile(path, shaderCode, "CS", "cs_" + SHADER_VERSION, configuration) : nullptr;
			free(configuration);

			ID3D11Device* device = Device::Get()->GetHandle();

			ID3D11VertexShader* vs = nullptr;
			ID3D11PixelShader* ps = nullptr;
			ID3D11ComputeShader* cs = nullptr;

			bool success = vsBlob || psBlob || csBlob;
			if (vsBlob) success = success && SUCCEEDED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs));
			if (psBlob) success = success && SUCCEEDED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps));
			if (csBlob) success = success && SUCCEEDED(device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &cs));

			if (success)
			{
				implementation.VS = vs;
				implementation.PS = ps;
				implementation.CS = cs;

				if (vsBlob)
				{
					implementation.IL.Reset();
					implementation.MIL.Reset();
					implementation.IL = CreateInputLayout(vsBlob, false);
					implementation.MIL = CreateInputLayout(vsBlob, true);
				}
			}
			else
			{
				SAFE_RELEASE(vs);
				SAFE_RELEASE(ps);
				SAFE_RELEASE(cs);
			}

			SAFE_RELEASE(vsBlob);
			SAFE_RELEASE(psBlob);
			SAFE_RELEASE(csBlob);

			return success;
		}
    }
    
	ShaderID CreateShader(const std::string& path)
	{
		ShaderID id;
		Shader& shader = Storage::CreateShader(id);
        shader.Path = path;
        return id;
	}

    static uint32_t GetImlementationHash(const std::vector<std::string>& defines, uint32_t shaderStages)
    {
        // TODO: Sort defines so we don't make duplicates
		std::string defAll = " "; 
        for (const std::string& def : defines) defAll += def;
        if (shaderStages & VS) defAll += "VS";
        if (shaderStages & PS) defAll += "PS";
        if (shaderStages & CS) defAll += "CS";
		return Hash::Crc32(defAll);
    }

	const ShaderImplementation& GetShaderImplementation(ShaderID shaderID, const std::vector<std::string>& defines, uint32_t shaderStages)
	{
        const uint32_t implHash = GetImlementationHash(defines, shaderStages);
        const Shader& shader = Storage::GetShader(shaderID);
		if (!shader.Implementations.contains(implHash))
		{
			Shader& modShader = const_cast<Shader&>(shader);
			bool success = CompileShader(modShader.Path, shaderStages, defines, modShader.Implementations[implHash]);
			ASSERT(success, "Shader compilation failed!");
		}
		return shader.Implementations.at(implHash);
	}

	
}