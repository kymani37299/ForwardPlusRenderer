#include "RenderResources.h"

#include "Render/Device.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"

namespace GFX
{
	RenderingResources RenderResources;

	void InitRenderingResources()
	{
		RenderResources.CopyShader = ScopedRef<Shader>(new Shader{ "Engine/Render/copy.hlsl" });

		struct FCVert
		{
			Float2 Position;
			Float2 UV;
		};

		const std::vector<FCVert> fcVBData = {
			FCVert{	{1.0,1.0},		{1.0,0.0}},
			FCVert{	{-1.0,-1.0},	{0.0,1.0}},
			FCVert{	{1.0,-1.0},		{1.0,1.0}},
			FCVert{	{1.0,1.0},		{1.0,0.0}},
			FCVert{ {-1.0,1.0},		{0.0,0.0}},
			FCVert{	{-1.0,-1.0},	{0.0,1.0}}
		};

		ResourceInitData initData = { &Device::Get()->GetContext(), fcVBData.data()};

		RenderResources.QuadBuffer = ScopedRef<Buffer>(GFX::CreateBuffer((uint32_t)fcVBData.size() * sizeof(FCVert), sizeof(FCVert), RCF_None, &initData));
		GFX::SetDebugName(RenderResources.QuadBuffer.get(), "Device::QuadBuffer");


	}

	void DestroyRenderingResources()
	{
		RenderResources.CopyShader = nullptr;
		RenderResources.QuadBuffer = nullptr;
	}
}

