#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include <DirectXMath.h>

#include "Utility/MathUtility.h"

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

	friend Float2 operator+(float l, const Float2& r) { return r + Float2(l, l); }
	friend Float2 operator-(float l, const Float2& r) { return r - Float2(l, l); }
	friend Float2 operator*(float l, const Float2& r) { return r * Float2(l, l); }
	friend Float2 operator/(float l, const Float2& r) { return r / Float2(l, l); }
	friend Float2 operator/(Float2 l, const float& r) { return l / Float2(r, r); }

	std::string ToString() const { return "(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }
	DirectX::XMVECTOR ToXM() const { return DirectX::XMVectorSet(x, y, 0.0f, 0.0f); }
	DirectX::XMFLOAT2 ToXMF() const { return DirectX::XMFLOAT2{ x,y }; }
	DirectX::XMFLOAT2A ToXMFA() const { return DirectX::XMFLOAT2A{ x,y }; }

	float Length() const { return Float2(DirectX::XMVector2Length(ToXM())).x; }
	float LengthFast() const { return Float2(DirectX::XMVector2LengthEst(ToXM())).x; }
	float LengthSq() const { return Float2(DirectX::XMVector2LengthSq(ToXM())).x; }
	float Dot(const Float2& other) const { return Float2(DirectX::XMVector2Dot(ToXM(), other.ToXM())).x; }
	Float2 Normalize() const { return Float2(DirectX::XMVector2Normalize(ToXM())); }
	Float2 NormalizeFast() const { return Float2(DirectX::XMVector2NormalizeEst(ToXM())); }
	Float2 Abs() const { return Float2{ std::abs(x), std::abs(y) }; }

	float SumElements() const { return x + y; }

	operator DirectX::XMVECTOR() const { return ToXM(); }
};

struct Float3
{
	Float3() : x(0), y(0), z(0) {}
	Float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
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
	DirectX::XMFLOAT3 ToXMF() const { return DirectX::XMFLOAT3{ x,y,z }; }
	DirectX::XMFLOAT3A ToXMFA() const { return DirectX::XMFLOAT3A{ x,y,z }; }

	float Length() const { return Float3(DirectX::XMVector3Length(ToXM())).x; }
	float LengthFast() const { return Float3(DirectX::XMVector3LengthEst(ToXM())).x; }
	float LengthSq() const { return Float3(DirectX::XMVector3LengthSq(ToXM())).x; }
	float Dot(const Float3& other) const { return Float3(DirectX::XMVector3Dot(ToXM(), other.ToXM())).x; }
	Float3 Normalize() const { return Float3(DirectX::XMVector3Normalize(ToXM())); }
	Float3 NormalizeFast() const { return Float3(DirectX::XMVector3NormalizeEst(ToXM())); }
	Float3 Cross(const Float3& other) const { return Float3(DirectX::XMVector3Cross(ToXM(), other.ToXM())); }
	Float3 Abs() const { return Float3{ std::abs(x), std::abs(y), std::abs(z) }; }

	float SumElements() const { return x + y + z; }

	operator DirectX::XMVECTOR() const { return ToXM(); }
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

	float Length() const { return Float4(DirectX::XMVector4Length(ToXM())).x; }
	float LengthFast() const { return Float4(DirectX::XMVector4LengthEst(ToXM())).x; }
	float LengthSq() const { return Float4(DirectX::XMVector4LengthSq(ToXM())).x; }
	float Dot(const Float4& other) const { return Float4(DirectX::XMVector4Dot(ToXM(), other.ToXM())).x; }
	Float4 Normalize() const { return Float4(DirectX::XMVector4Normalize(ToXM())); }
	Float4 NormalizeFast() const { return Float4(DirectX::XMVector4NormalizeEst(ToXM())); }
	Float4 Abs() const { return Float4{ std::abs(x), std::abs(y), std::abs(z), std::abs(w) }; }

	float SumElements() const { return x + y + z + w; }

	operator DirectX::XMVECTOR() const { return ToXM(); }
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

namespace XMUtility
{
	inline DirectX::XMFLOAT4X4 ToXMFloat4x4(DirectX::XMMATRIX xm)
	{
		using namespace DirectX;
		XMFLOAT4X4 value;
		XMStoreFloat4x4(&value, xm);
		return value;
	}

	inline DirectX::XMFLOAT4X4 ToHLSLFloat4x4(DirectX::XMMATRIX matrix)
	{
		using namespace DirectX;
		matrix = XMMatrixTranspose(matrix);
		return ToXMFloat4x4(matrix);
	}
}

class BitField
{
public:
	BitField() { }
	BitField(uint32_t numBits) { Resize(numBits); }

	void Resize(uint32_t numBits)
	{
		m_NumBits = numBits;
		m_NumElements = (m_NumBits + NumBitsPerElement - 1) / NumBitsPerElement;
		m_Data.resize(m_NumElements);
	}

	void Reset()
	{
		m_Data.clear();
		m_Data.resize(m_NumElements);
	}

	void Set(uint32_t index, bool value)
	{
		const uint32_t elementIndex = index / NumBitsPerElement;
		const uint32_t bitIndex = index % NumBitsPerElement;
		const uint32_t writeValue = value ? 1 : 0;
		uint32_t& dataValue = m_Data[elementIndex];
		dataValue &= ~(1 << bitIndex);
		dataValue |= writeValue << bitIndex;
	}

	bool Get(uint32_t index) const
	{
		const uint32_t elementIndex = index / NumBitsPerElement;
		const uint32_t bitIndex = index % NumBitsPerElement;
		return m_Data[elementIndex] & (1 << bitIndex);
	}

	void* GetRaw() const
	{
		return (void*) m_Data.data();
	}

	uint32_t CountOnes()
	{
		static bool lookupInitialized = false;
		static uint8_t lookupTable[1 << 8];
		if (!lookupInitialized)
		{
			for (uint32_t i = 0; i < (1 << 8); i++)
			{
				uint8_t bitCount = 0;
				for (uint8_t j = 0; j < 8; j++)
				{
					if ((i & (1 << j)) > 0) bitCount++;
				}
				lookupTable[i] = bitCount;
			}
			lookupInitialized = true;
		}
		const uint8_t* bytePtr = reinterpret_cast<uint8_t*>(m_Data.data());
		const uint32_t byteSize = MathUtility::CeilDiv(m_NumBits, 8);
		uint32_t bitCount = 0;
		for (uint32_t i = 0; i < byteSize; i++) bitCount += lookupTable[bytePtr[i]];
		return bitCount;
	}

private:
	static constexpr uint32_t NumBitsPerElement = sizeof(uint32_t) * 8;
	uint32_t m_NumElements = 0;
	uint32_t m_NumBits = 0;
	std::vector<uint32_t> m_Data;
};