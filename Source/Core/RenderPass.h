#pragma once

#include <string>

struct ID3D11DeviceContext1;

class RenderPass
{
public:
	virtual std::string GetPassName() const { return "RenderPass"; }
	
	virtual void OnInit(ID3D11DeviceContext1* context) {};
	virtual void OnDestroy(ID3D11DeviceContext1* context) {}
	
	virtual void OnDraw(ID3D11DeviceContext1* context) {};
	virtual void OnUpdate(ID3D11DeviceContext1* context, float dt) {}

	virtual void OnShaderReload(ID3D11DeviceContext1* context) {}
	virtual void OnWindowResize(ID3D11DeviceContext1* context) {}

};