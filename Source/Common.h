#pragma once

#include <iostream>
#include <string>

#include "Utility/DataTypes.h"

#define SAFE_DELETE(X) if((X)) { delete (X); (X) = nullptr; }
#define UNUSED(x) (void)(x)

#define JOIN(X,Y) X##Y

#define DELETE_COPY_CONSTRUCTOR(X) \
X(X const&) = delete; \
X& operator=(X const&) = delete;

#ifdef DEBUG
#define ASSERT(X,msg) if(!(X)) { std::cout << msg << std::endl; __debugbreak(); }
#define NOT_IMPLEMENTED ASSERT(0, "NOT IMPLEMENTED")
#else
#define ASSERT(X, MSG)
#define NOT_IMPLEMENTED
#endif // DEBUG

#define FORCE_CRASH *((unsigned int*)0) = 0xDEAD

#define STATIC_ARRAY_SIZE(X) (sizeof(X)/(sizeof(X[0])))

template<typename T> T MAX(const T & a, const T & b) { return a > b ? a : b; }
template<typename T> T MIN(const T & a, const T & b) { return a < b ? a : b; }
template<typename T> inline static constexpr uint32_t EnumToInt(T enumValue) { return static_cast<uint32_t>(enumValue); }
template<typename T> inline static constexpr T IntToEnum(uint32_t intValue) { return static_cast<T>(intValue); }