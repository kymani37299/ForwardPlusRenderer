#pragma once

struct GraphicsContext;
struct Texture;

class Application
{
public:
	virtual ~Application() {};

	virtual void OnInit(GraphicsContext& context) {}
	virtual void OnDestroy(GraphicsContext& context) {}

	virtual Texture* OnDraw(GraphicsContext& context) { return nullptr; }
	virtual void OnUpdate(GraphicsContext& context, float dt) {}

	virtual void OnShaderReload(GraphicsContext& context) {}
	virtual void OnWindowResize(GraphicsContext& context) {}
};