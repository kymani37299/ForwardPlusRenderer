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
	Float2() : x(0), y(0) {}
	Float2(float _x, float _y) : x(_x), y(_y) {}
	Float2(DirectX::XMFLOAT3 xm) : x(xm.x), y(xm.y) {}
	Float2(DirectX::XMVECTOR xm)
	{
		DirectX::XMFLOAT2 xmf;
		DirectX::XMStoreFloat2(&xmf, xm);
		x = xmf.x;
		y = xmf.y;
	}

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

	std::string ToString() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }
	DirectX::XMVECTOR ToXM() const { return DirectX::XMVectorSet(x, y, 0.0f, 0.0f); }
	DirectX::XMFLOAT2 ToXMF() const { return DirectX::XMFLOAT2{ x,y }; }
	DirectX::XMFLOAT2A ToXMFA() const { return DirectX::XMFLOAT2A{ x,y }; }
};

struct Float3
{
	Float3() : x(0), y(0), z(0) {}
	Float3(float _x, float _y, float _z): x(_x), y(_y), z(_z) {}
	Float3(DirectX::XMFLOAT3 xm) : x(xm.x), y(xm.y), z(xm.z) {}
	Float3(DirectX::XMVECTOR xm)
	{
		DirectX::XMFLOAT3 xmf;
		DirectX::XMStoreFloat3(&xmf, xm);
		x = xmf.x;
		y = xmf.y;
		z = xmf.z;
	}

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

	std::string ToString() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")"; }
	DirectX::XMVECTOR ToXM() const { return DirectX::XMVectorSet(x, y, z, 0.0f); }
	DirectX::XMFLOAT3 ToXMF() const { return DirectX::XMFLOAT3{x,y,z}; }
	DirectX::XMFLOAT3A ToXMFA() const { return DirectX::XMFLOAT3A{x,y,z}; }
};

struct Float4
{
	Float4() : x(0), y(0), z(0), w(0) {}
	Float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
	Float4(DirectX::XMFLOAT4 xm) : x(xm.x), y(xm.y), z(xm.z), w(xm.w) {}
	Float4(DirectX::XMVECTOR xm)
	{
		DirectX::XMFLOAT4 xmf;
		DirectX::XMStoreFloat4(&xmf, xm);
		x = xmf.x;
		y = xmf.y;
		z = xmf.z;
		w = xmf.w;
	}

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

	std::string ToString() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ", " + std::to_string(w) + ")"; }
	DirectX::XMVECTOR ToXM() const { return DirectX::XMVectorSet(x, y, z, w); }
	DirectX::XMFLOAT4 ToXMF() const { return DirectX::XMFLOAT4{ x,y,z,w }; }
	DirectX::XMFLOAT4A ToXMFA() const { return DirectX::XMFLOAT4A{ x,y,z,w }; }
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

