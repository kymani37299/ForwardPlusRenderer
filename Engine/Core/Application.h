#pragma once

#include "Render/ResourceID.h"

struct ID3D11DeviceContext;

class Application
{
public:
	virtual void OnInit(ID3D11DeviceContext* context) {}
	virtual void OnDestroy(ID3D11DeviceContext* context) {}

	virtual TextureID OnDraw(ID3D11DeviceContext* context) { return TextureID{}; }
	virtual void OnUpdate(ID3D11DeviceContext* context, float dt) {}

	virtual void OnShaderReload(ID3D11DeviceContext* context) {}
	virtual void OnWindowResize(ID3D11DeviceContext* context) {}
};