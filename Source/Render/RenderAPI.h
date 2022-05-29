#pragma once

#include "Common.h"

#include <wrl.h>
#include <d3d11_1.h>
#include <DirectXMesh/DirectXMesh.h>

#ifdef DEBUG
#define API_CALL(X) {HRESULT hr = X; ASSERT(SUCCEEDED(hr), "DX ERROR: 0x" << std::hex << hr);}
#else
#define API_CALL(X) X
#endif // DEBUG

#define SAFE_RELEASE(X) { if(X) X->Release(); }

#define MESHLET_INDEX_COUNT 384

using Microsoft::WRL::ComPtr;
