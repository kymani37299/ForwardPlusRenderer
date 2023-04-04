#pragma once

#include <iostream>
#include <string>
#include <Optick/optick.h>

#include "Utility/DataTypes.h"

#define SAFE_DELETE(X) if((X)) { delete (X); (X) = nullptr; }
#define UNUSED(x) (void)(x)

#define FORCE_CRASH *((unsigned int*)0) = 0xDEAD

#ifdef DEBUG
#define ASSERT(X,msg) if(!(X)) { std::cout << msg << std::endl; __debugbreak(); }
#define NOT_IMPLEMENTED ASSERT(0, "NOT IMPLEMENTED")
#else
#define ASSERT(X, MSG) {}
#define NOT_IMPLEMENTED {}
#endif // DEBUG

#define ASSERT_CORE(X, msg) if(!(X)) { std::cout << msg << std::endl; FORCE_CRASH; }

#define STATIC_ARRAY_SIZE(X) (sizeof(X)/(sizeof(X[0])))

template<typename T> T MAX(const T & a, const T & b) { return a > b ? a : b; }
template<typename T> T MIN(const T & a, const T & b) { return a < b ? a : b; }
template<typename T> inline static constexpr uint32_t EnumToInt(T enumValue) { return static_cast<uint32_t>(enumValue); }
template<typename T> inline static constexpr T IntToEnum(uint32_t intValue) { return static_cast<T>(intValue); }

template<typename T> using ScopedRef = std::unique_ptr<T>;
template<typename T> using Ref = std::shared_ptr<T>;

#define DEFINE_ENUM_CLASS_FLAGS_EX(T, T_PARENT)																			\
enum class T : T_PARENT;																								\
inline T operator & (T a, T b)		{ return static_cast<T>(static_cast<T_PARENT>(a) & static_cast<T_PARENT>(b));};		\
inline T operator |	(T a, T b)		{ return static_cast<T>(static_cast<T_PARENT>(a) | static_cast<T_PARENT>(b)); };	\
inline T operator ^	(T a, T b)		{ return static_cast<T>(static_cast<T_PARENT>(a) ^ static_cast<T_PARENT>(b)); };	\
inline T operator ~	(T a)			{ return static_cast<T>(~static_cast<T_PARENT>(a)); };								\
inline T& operator &= (T& a, T b)	{ a = a & b;	return a;};															\
inline T& operator |= (T& a, T b)	{ a = a | b;	return a;};															\
inline T& operator ^= (T& a, T b)	{ a = a ^ b;	return a;};															\
inline bool TestFlag(T a, T b)		{ return static_cast<T_PARENT>(a & b) != 0; }

#define DEFINE_ENUM_CLASS_FLAGS(T) DEFINE_ENUM_CLASS_FLAGS_EX(T, uint32_t)

#define MACRO_CONCAT_IMPL(x, y) x##y
#define MACRO_CONCAT(x, y) MACRO_CONCAT_IMPL(x, y)

struct GraphicsContext;

namespace EnginePrivate
{
	class ScopedSectionResolver
	{
	public:
		ScopedSectionResolver(const GraphicsContext& context, const std::string& sectonName);
		~ScopedSectionResolver();

	private:
		const GraphicsContext& m_Context;
	};
}

#define PROFILE_SECTION_CPU(SECTION_NAME) OPTICK_EVENT(SECTION_NAME)
#define PROFILE_SECTION(GFX_CONTEXT, SECTION_NAME)  OPTICK_GPU_EVENT(SECTION_NAME); EnginePrivate::ScopedSectionResolver MACRO_CONCAT(_sectionResolver, __LINE__){GFX_CONTEXT, SECTION_NAME};
#define PROFILE_FUNCTION() OPTICK_GPU_EVENT(__FUNCTION__)