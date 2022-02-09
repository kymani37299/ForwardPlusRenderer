#pragma once

#include "Common.h"

#include <wrl.h>
#include <d3d11_1.h>

#ifdef DEBUG
#define API_CALL(X) {HRESULT hr = X; ASSERT(SUCCEEDED(hr), "DX ERROR");}
#else
#define API_CALL(X) X
#endif // DEBUG

using Microsoft::WRL::ComPtr;