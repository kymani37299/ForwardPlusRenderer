#include "RenderResources.h"

#include "Render/Context.h"
#include "Render/Device.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"

namespace GFX
{
	RenderingResources RenderResources;

	void InitRenderingResources(GraphicsContext& context)
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

		ResourceInitData initData = { &context, fcVBData.data()};

		RenderResources.QuadBuffer = ScopedRef<Buffer>(GFX::CreateBuffer((uint32_t)fcVBData.size() * sizeof(FCVert), sizeof(FCVert), RCF::None, &initData));
		GFX::SetDebugName(RenderResources.QuadBuffer.get(), "Device::QuadBuffer");


	}

	void DestroyRenderingResources(GraphicsContext& context)
	{
		RenderResources.CopyShader = nullptr;
		RenderResources.QuadBuffer = nullptr;
	}
}

