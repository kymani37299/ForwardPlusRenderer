#pragma once

#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <DirectXMesh/DirectXMesh.h>

#include "Common.h"
#include "Render/D3D12MemAlloc.h"

#ifdef DEBUG
#define API_CALL(X) {HRESULT hr = X; ASSERT(SUCCEEDED(hr), "DX ERROR " __FILE__);}
#else
#define API_CALL(X) X
#endif // DEBUG

using Microsoft::WRL::ComPtr;

#define USE_PIX

inline void MemcpySubresource(
	_In_ const D3D12_MEMCPY_DEST* pDest,
	_In_ const D3D12_SUBRESOURCE_DATA* pSrc,
	SIZE_T RowSizeInBytes,
	UINT NumRows,
	UINT NumSlices)
{
	for (UINT z = 0; z < NumSlices; ++z)
	{
		BYTE* pDestSlice = reinterpret_cast<BYTE*>(pDest->pData) + pDest->SlicePitch * z;
		const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(pSrc->pData) + pSrc->SlicePitch * z;
		for (UINT y = 0; y < NumRows; ++y)
		{
			memcpy(pDestSlice + pDest->RowPitch * y,
				pSrcSlice + pSrc->RowPitch * y,
				RowSizeInBytes);
		}
	}
}