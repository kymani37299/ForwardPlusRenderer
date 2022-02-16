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
	RCF_None = 0,

	// Bindings
	RCF_Bind_VB = 1 << 0,
	RCF_Bind_IB = RCF_Bind_VB << 1,
	RCF_Bind_CB = RCF_Bind_IB << 1,
	RCF_Bind_SB = RCF_Bind_CB << 1,
	RCF_Bind_SRV = RCF_Bind_SB << 1,
	RCF_Bind_UAV = RCF_Bind_SRV << 1,
	RCF_Bind_RTV = RCF_Bind_UAV << 1,
	RCF_Bind_DSV = RCF_Bind_RTV << 1,

	// Access
	RCF_CPU_Write = RCF_Bind_DSV << 1,
	RCF_CPU_Read = RCF_CPU_Write << 1,

	// Misc
	RCF_Staging = RCF_CPU_Read << 1,
	RCF_GenerateMips = RCF_Staging << 1
};

enum ShaderCreationFlags : uint32_t
{
	SCF_None = 0,

	SCF_VS = 1 << 0,
	SCF_PS = SCF_VS << 1,
};

