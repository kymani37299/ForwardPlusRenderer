#pragma once

#define SAFE_DELETE(X) if((X)) { delete (X); (X) = nullptr; }
#define UNUSED(x) (void)(x)

#define JOIN(X,Y) X##Y

#define DELETE_COPY_CONSTRUCTOR(X) \
X(X const&) = delete; \
X& operator=(X const&) = delete;

#ifdef DEBUG
#define ASSERT(X,msg) if(!(X)) { POPUP("ASSERT: " msg); __debugbreak(); }
#define NOT_IMPLEMENTED ASSERT(0, "NOT IMPLEMENTED")
#else
#define ASSERT(X, MSG)
#define NOT_IMPLEMENTED
#endif // DEBUG

struct Float2
{
	float x;
	float y;
};

struct Float3
{
	float x;
	float y;
	float z;
};

struct Float4
{
	float x;
	float y;
	float z;
	float w;
};

struct Mat3x3
{
	Float3 row[3];
};

struct Mat4x4
{
	Float4 row[4];
};