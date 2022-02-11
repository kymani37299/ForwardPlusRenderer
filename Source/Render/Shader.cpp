#include "Shader.h"

#include <set>
#include <vector>
#include <fstream>
#include <d3dcompiler.h>

#include "Render/Device.h"
#include "Render/Resource.h"
#include "Utility/StringUtility.h"
#include "Utility/PathUtility.h"

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

        ID3DBlob* ReadBlobFromFile(const std::string& shaderCode, const std::string& entry, const std::string& hlsl_target, D3D_SHADER_MACRO* configuration)
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
                std::cout << errorString << std::endl;
                ASSERT(0, "Shader compiler error!");
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
    }
    
    static const std::string SHADER_VERSION = "5_0";

	ShaderID CreateShader(const std::string& path)
	{
		ShaderID id;
		Shader& shader = Storage::CreateShader(id);

		std::string shaderCode;
        ReadShaderFile(path, shaderCode);

        ID3DBlob* vsBlob = ReadBlobFromFile(shaderCode, "VS", "vs_" + SHADER_VERSION, nullptr);
        ID3DBlob* psBlob = ReadBlobFromFile(shaderCode, "PS", "ps_" + SHADER_VERSION, nullptr);

        ID3D11Device* device = Device::Get()->GetHandle();

        bool success = vsBlob || psBlob;
        if(vsBlob) success = success && SUCCEEDED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, shader.VS.GetAddressOf()));
        if(psBlob) success = success && SUCCEEDED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, shader.PS.GetAddressOf()));

        ASSERT(success, "[CreateShader] Shader compilation failed!");

        if (vsBlob)
        {
            shader.IL = CreateInputLayout(vsBlob, false);
            shader.MIL = CreateInputLayout(vsBlob, true);
        }

        SAFE_RELEASE(vsBlob);
        SAFE_RELEASE(psBlob);

        return id;
	}
}