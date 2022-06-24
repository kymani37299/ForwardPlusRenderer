#pragma once

#include "Common.h"

struct BufferID
{
	static constexpr uint32_t InvalidID = static_cast<uint32_t>(-1);
	uint32_t ID = InvalidID;
	bool Valid() const { return ID != InvalidID; }
};

struct TextureID
{
	static constexpr uint32_t InvalidID = static_cast<uint32_t>(-1);
	uint32_t ID = InvalidID;
	bool Valid() const { return ID != InvalidID; }
};

enum ShaderStage : uint32_t
{
	VS = 1,
	PS = 2,
	CS = 4
};

struct ShaderID
{
	static constexpr uint32_t InvalidID = static_cast<uint32_t>(-1);
	uint32_t ID = InvalidID;
	bool Valid() const { return ID != InvalidID; }
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
	RCF_CPU_Write_Persistent = RCF_CPU_Read << 1,

	// Misc
	RCF_Staging = RCF_CPU_Write_Persistent << 1,
	RCF_GenerateMips = RCF_Staging << 1,
	RCF_CopyDest = RCF_GenerateMips << 1,
	RCF_Cubemap = RCF_CopyDest << 1,

	// MSAA
	RCF_MSAA_X2 = RCF_Cubemap << 1,
	RCF_MSAA_X4 = RCF_MSAA_X2 << 1,
	RCF_MSAA_X8 = RCF_MSAA_X4 << 1,
	RCF_MSAA_X16 = RCF_MSAA_X8 << 1,
};

