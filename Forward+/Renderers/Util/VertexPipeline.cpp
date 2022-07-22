#include "VertexPipeline.h"

#include <Engine/Render/Buffer.h>
#include <Engine/Render/Commands.h>

#include "Globals.h"
#include "Scene/SceneGraph.h"
#include "Shaders/shared_definitions.h"

VertexPipeline* VertPipeline = nullptr;

bool IsMeshletVisible(const Drawable& drawable, uint32_t meshletIndex, RenderGroup& rg)
{
	if (DebugToolsConfig.DisableGeometryCulling || !DebugToolsConfig.UseMeshletCulling) return true;

	static ViewFrustum vf;

	const Mesh& m = rg.Meshes[drawable.MeshIndex];
	const Entity& e = MainSceneGraph->Entities[drawable.EntityIndex];

	if (!DebugToolsConfig.FreezeGeometryCulling)
		vf = MainSceneGraph->MainCamera.CameraFrustum;

	BoundingSphere bv;
	bv.Center = m.MeshletCullData[meshletIndex].BoundingSphere.Center;
	bv.Radius = m.MeshletCullData[meshletIndex].BoundingSphere.Radius;
	bv = e.GetBoundingVolume(bv);
	
	return vf.IsInFrustum(bv);
}

VertexPipeline::VertexPipeline()
{

}

VertexPipeline::~VertexPipeline()
{

}

void VertexPipeline::Init(GraphicsContext& context)
{
	std::vector<uint32_t> indices{};
	indices.resize(MESHLET_INDEX_COUNT);
	for (uint32_t i = 0; i < MESHLET_INDEX_COUNT; i++) indices[i] = i;
	ResourceInitData meshletInitData = { &context, indices.data() };
	MeshletIndexBuffer = ScopedRef<Buffer>(GFX::CreateBuffer(MESHLET_INDEX_COUNT * sizeof(uint32_t), sizeof(uint32_t), RCF_None, &meshletInitData));
	DrawableInstanceBuffer = ScopedRef<Buffer>(GFX::CreateVertexBuffer<uint32_t>(1, nullptr));
	MeshletInstanceBuffer = ScopedRef<Buffer>(GFX::CreateVertexBuffer<uint32_t>(1, nullptr));
	IndexCount = MESHLET_INDEX_COUNT;

	GFX::SetDebugName(MeshletIndexBuffer.get(), "VertexPipeline::MeshletIndexBuffer");
	GFX::SetDebugName(DrawableInstanceBuffer.get(), "VertexPipeline::DrawableInstanceBuffer");
	GFX::SetDebugName(MeshletInstanceBuffer.get(), "VertexPipeline::MeshletInstanceBuffer");
}

void VertexPipeline::Draw(GraphicsContext& context, GraphicsState& state, RenderGroup& rg, bool skipCulling)
{
	// Prepare
	std::vector<uint32_t> meshlets;
	std::vector<uint32_t> drawables;
	for (uint32_t i = 0; i < rg.Drawables.GetSize(); i++)
	{
		if (skipCulling || rg.VisibilityMask.Get(i))
		{
			Drawable d = rg.Drawables[i];
			const Mesh& m = rg.Meshes[d.MeshIndex];
			uint32_t meshletCount = m.IndexCount / MESHLET_INDEX_COUNT;
			for (uint32_t j = 0; j < meshletCount; j++)
			{
				if (skipCulling || IsMeshletVisible(d, j, rg))
				{
					drawables.push_back(i);
					meshlets.push_back(j);
				}
			}
		}
	}
	InstanceCount = meshlets.size();

	// Upload
	GFX::ExpandBuffer(context, MeshletInstanceBuffer.get(), meshlets.size() * sizeof(uint32_t));
	GFX::ExpandBuffer(context, DrawableInstanceBuffer.get(), drawables.size() * sizeof(uint32_t));
	GFX::Cmd::UploadToBuffer(context, MeshletInstanceBuffer.get(), 0, meshlets.data(), 0, meshlets.size() * sizeof(uint32_t));
	GFX::Cmd::UploadToBuffer(context, DrawableInstanceBuffer.get(), 0, drawables.data(), 0, drawables.size() * sizeof(uint32_t));

	// Execute draw
	rg.SetupPipelineInputs(state);
	state.VertexBuffers.resize(3);
	state.VertexBuffers[0] = MeshletIndexBuffer.get();
	state.VertexBuffers[1] = MeshletInstanceBuffer.get();
	state.VertexBuffers[2] = DrawableInstanceBuffer.get();
	GFX::Cmd::BindState(context, state);
	context.CmdList->DrawInstanced(IndexCount, InstanceCount, 0, 0);
}
