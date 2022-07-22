#pragma once

#include <vector>
#include <unordered_map>
#include <set>

struct D3D12_INPUT_ELEMENT_DESC;

enum ShaderStage : uint8_t
{
	VS = 1 << 0,
	GS = 1 << 1,
	HS = 1 << 2,
	DS = 1 << 3,
	PS = 1 << 4,
	CS = 1 << 5,
	SHADER_STAGE_COUNT = 6
};

using ShaderHash = uint32_t;

struct CompiledShader
{
	D3D12_SHADER_BYTECODE Vertex;
	D3D12_SHADER_BYTECODE Geometry;
	D3D12_SHADER_BYTECODE Hull;
	D3D12_SHADER_BYTECODE Domain;
	D3D12_SHADER_BYTECODE Pixel;
	D3D12_SHADER_BYTECODE Compute;
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayoutMultiInput;

	std::vector<ComPtr<ID3DBlob>> Data;
};

struct Shader
{
	static std::set<Shader*> AllShaders;

	Shader(std::string path) : Path(path) 
	{
		AllShaders.insert(this);
	}

	~Shader()
	{
		AllShaders.erase(this);
	}

	std::string Path;
	std::unordered_map<ShaderHash, CompiledShader> Implementations;
};

namespace GFX
{
	const CompiledShader& GetCompiledShader(Shader* shaderID, const std::vector<std::string>& defines, uint32_t shaderStages);
	void ReloadAllShaders();
}