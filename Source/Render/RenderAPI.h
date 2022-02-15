#pragma once

#include "Common.h"

#include <wrl.h>
#include <d3d11_1.h>

#ifdef DEBUG
#define API_CALL(X) {HRESULT hr = X; ASSERT(SUCCEEDED(hr), "DX ERROR: " << hr);}
#else
#define API_CALL(X) X
#endif // DEBUG

#define SAFE_RELEASE(X) { if(X) X->Release(); }

using Microsoft::WRL::ComPtr;