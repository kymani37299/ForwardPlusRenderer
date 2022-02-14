#pragma once

#include "Common.h"

using BufferID = uint32_t;
using TextureID = uint32_t;
using ShaderID = uint32_t;

static constexpr TextureID TextureID_Invalid = static_cast<TextureID>(-1);
static constexpr BufferID BufferID_Invalid = static_cast<BufferID>(-1);
static constexpr ShaderID ShaderID_Invalid = static_cast<ShaderID>(-1);

struct RenderTargetID
{
	TextureID ColorTexture = TextureID_Invalid;
	TextureID DepthTexture = TextureID_Invalid;
};

enum ResourceCreationFlags : uint64_t
{
	None = 0,

	// Bindings
	RCF_Bind_VB = 1 << 0,
	RCF_Bind_IB = RCF_Bind_VB << 1,
	RCF_Bind_CB = RCF_Bind_IB << 1,
	RCF_Bind_SB = RCF_Bind_CB << 1,
	RCF_Bind_UAV = RCF_Bind_SB << 1,

	// Access
	RCF_CPU_Write = RCF_Bind_UAV << 1,
	RCF_CPU_Read = RCF_CPU_Write << 1,
};

