#pragma once

#include <iostream>
#include <string>
#include <stdint.h>
#include <DirectXMath.h>

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

struct Float2
{
	float x;
	float y;

	Float2& operator+=(const Float2& x) { this->x += x.x; this->y += x.y; return *this; }
	Float2& operator-=(const Float2& x) { this->x -= x.x; this->y -= x.y; return *this; }
	Float2& operator*=(const Float2& x) { this->x *= x.x; this->y *= x.y; return *this; }
	Float2& operator/=(const Float2& x) { this->x /= x.x; this->y /= x.y; return *this; }

	Float2& operator+=(const float& x) { this->x += x; this->y += x; return *this; }
	Float2& operator-=(const float& x) { this->x -= x; this->y -= x; return *this; }
	Float2& operator*=(const float& x) { this->x *= x; this->y *= x; return *this; }
	Float2& operator/=(const float& x) { this->x /= x; this->y /= x; return *this; }

	friend Float2 operator+(Float2 l, const Float2& r) { l += r; return l; }
	friend Float2 operator-(Float2 l, const Float2& r) { l -= r; return l; }
	friend Float2 operator*(Float2 l, const Float2& r) { l *= r; return l; }
	friend Float2 operator/(Float2 l, const Float2& r) { l /= r; return l; }

	friend Float2 operator+(float l, const Float2& r) { return r + Float2(l,l); }
	friend Float2 operator-(float l, const Float2& r) { return r - Float2(l,l); }
	friend Float2 operator*(float l, const Float2& r) { return r * Float2(l,l); }
	friend Float2 operator/(float l, const Float2& r) { return r / Float2(l,l); }
	friend Float2 operator/(Float2 l, const float& r) { return l / Float2(r,r); }

	DirectX::XMVECTOR ToXM() { return DirectX::XMVectorSet(x, y, 0.0f, 0.0f); }
};

struct Float3
{
	float x;
	float y;
	float z;

	Float3& operator+=(const Float3& x) { this->x += x.x; this->y += x.y; this->z += x.z; return *this; }
	Float3& operator-=(const Float3& x) { this->x -= x.x; this->y -= x.y; this->z -= x.z; return *this; }
	Float3& operator*=(const Float3& x) { this->x *= x.x; this->y *= x.y; this->z *= x.z; return *this; }
	Float3& operator/=(const Float3& x) { this->x /= x.x; this->y /= x.y; this->z /= x.z; return *this; }

	Float3& operator+=(const float& x) { this->x += x; this->y += x; this->z += x; return *this; }
	Float3& operator-=(const float& x) { this->x -= x; this->y -= x; this->z -= x; return *this; }
	Float3& operator*=(const float& x) { this->x *= x; this->y *= x; this->z *= x; return *this; }
	Float3& operator/=(const float& x) { this->x /= x; this->y /= x; this->z /= x; return *this; }

	friend Float3 operator+(Float3 l, const Float3& r) { l += r; return l; }
	friend Float3 operator-(Float3 l, const Float3& r) { l -= r; return l; }
	friend Float3 operator*(Float3 l, const Float3& r) { l *= r; return l; }
	friend Float3 operator/(Float3 l, const Float3& r) { l /= r; return l; }

	friend Float3 operator+(float l, const Float3& r) { return r + Float3(l, l, l); }
	friend Float3 operator-(float l, const Float3& r) { return r - Float3(l, l, l); }
	friend Float3 operator*(float l, const Float3& r) { return r * Float3(l, l, l); }
	friend Float3 operator/(float l, const Float3& r) { return r / Float3(l, l, l); }
	friend Float3 operator/(Float3 l, const float& r) { return l / Float3(r, r, r); }

	DirectX::XMVECTOR ToXM() { return DirectX::XMVectorSet(x, y, z, 0.0f); }
};

struct Float4
{
	float x;
	float y;
	float z;
	float w;

	Float4& operator+=(const Float4& x) { this->x += x.x; this->y += x.y; this->z += x.z; this->w += x.w; return *this; }
	Float4& operator-=(const Float4& x) { this->x -= x.x; this->y -= x.y; this->z -= x.z; this->w -= x.w; return *this; }
	Float4& operator*=(const Float4& x) { this->x *= x.x; this->y *= x.y; this->z *= x.z; this->w *= x.w; return *this; }
	Float4& operator/=(const Float4& x) { this->x /= x.x; this->y /= x.y; this->z /= x.z; this->w /= x.w; return *this; }

	Float4& operator+=(const float& x) { this->x += x; this->y += x; this->z += x; this->w += x; return *this; }
	Float4& operator-=(const float& x) { this->x -= x; this->y -= x; this->z -= x; this->w -= x; return *this; }
	Float4& operator*=(const float& x) { this->x *= x; this->y *= x; this->z *= x; this->w *= x; return *this; }
	Float4& operator/=(const float& x) { this->x /= x; this->y /= x; this->z /= x; this->w /= x; return *this; }

	friend Float4 operator+(Float4 l, const Float4& r) { l += r; return l; }
	friend Float4 operator-(Float4 l, const Float4& r) { l -= r; return l; }
	friend Float4 operator*(Float4 l, const Float4& r) { l *= r; return l; }
	friend Float4 operator/(Float4 l, const Float4& r) { l /= r; return l; }

	friend Float4 operator+(float l, const Float4& r) { return r + Float4(l, l, l, l); }
	friend Float4 operator-(float l, const Float4& r) { return r - Float4(l, l, l, l); }
	friend Float4 operator*(float l, const Float4& r) { return r * Float4(l, l, l, l); }
	friend Float4 operator/(float l, const Float4& r) { return r / Float4(l, l, l, l); }
	friend Float4 operator/(Float4 l, const float& r) { return l / Float4(r, r, r, r); }

	DirectX::XMVECTOR ToXM() { return DirectX::XMVectorSet(x, y, z, w); }
};

struct ColorUNORM
{
	ColorUNORM() : r(0), g(0), b(0), a(0) {}

	// [0-255]
	ColorUNORM(unsigned char r, unsigned char g, unsigned char b, unsigned char a) : r(r), g(g), b(b), a(a) {}

	// [0.0f - 1.0f]
	ColorUNORM(float r, float g, float b, float a) : r(toU8(r)), g(toU8(g)), b(toU8(b)), a(toU8(a)) {}

	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;

private:
	static unsigned char toU8(float x) { return (unsigned char)(255.0f * x); }
};

struct Mat3x3
{
	Float3 row[3];
};

struct Mat4x4
{
	Float4 row[4];
};