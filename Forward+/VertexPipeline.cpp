#include "VertexPipeline.h"

#include <Engine/Render/Buffer.h>
#include <Engine/Render/Commands.h>
#include <Engine/Gui/GUI_Implementations.h>
#include <Engine/Core/SceneGraph.h>

VertexPipeline VertPipeline;

bool IsMeshletVisible(const Drawable& drawable, uint32_t meshletIndex, RenderGroup& rg)
{
	if (DebugToolsConfig.DisableGeometryCulling) return true;

	static ViewFrustum vf;

	const Mesh& m = rg.Meshes[drawable.MeshIndex];
	const Entity& e = MainSceneGraph.Entities[drawable.EntityIndex];

	if (!DebugToolsConfig.FreezeGeometryCulling)
		vf = MainSceneGraph.MainCamera.CameraFrustum;

	BoundingSphere bv;
	bv.Center = m.MeshletCullData[meshletIndex].BoundingSphere.Center;
	bv.Radius = m.MeshletCullData[meshletIndex].BoundingSphere.Radius;
	bv = e.GetBoundingVolume(bv);
	
	return vf.IsInFrustum(bv);
}

void VertexPipeline::Init(ID3D11DeviceContext* context)
{
	std::vector<uint32_t> indices{};
	indices.resize(MESHLET_INDEX_COUNT);
	for (uint32_t i = 0; i < MESHLET_INDEX_COUNT; i++) indices[i] = i;
	MeshletIndexBuffer = GFX::CreateBuffer(MESHLET_INDEX_COUNT * sizeof(uint32_t), sizeof(uint32_t), RCF_Bind_VB, indices.data());
	DrawableInstanceBuffer = GFX::CreateVertexBuffer<uint32_t>(1, nullptr);
	MeshletInstanceBuffer = GFX::CreateVertexBuffer<uint32_t>(1, nullptr);
	IndexCount = MESHLET_INDEX_COUNT;
}

void VertexPipeline::Draw(ID3D11DeviceContext* context, RenderGroup& rg, bool skipCulling)
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
	GFX::ExpandBuffer(context, MeshletInstanceBuffer, meshlets.size() * sizeof(uint32_t));
	GFX::ExpandBuffer(context, DrawableInstanceBuffer, drawables.size() * sizeof(uint32_t));
	GFX::Cmd::UploadToBuffer(context, MeshletInstanceBuffer, 0, meshlets.data(), 0, meshlets.size() * sizeof(uint32_t));
	GFX::Cmd::UploadToBuffer(context, DrawableInstanceBuffer, 0, drawables.data(), 0, drawables.size() * sizeof(uint32_t));

	// Execute draw
	rg.SetupPipelineInputs(context);
	GFX::Cmd::BindVertexBuffers(context, { VertPipeline.MeshletIndexBuffer, VertPipeline.MeshletInstanceBuffer, VertPipeline.DrawableInstanceBuffer });
	context->DrawInstanced(VertPipeline.IndexCount, VertPipeline.InstanceCount, 0, 0);
}
