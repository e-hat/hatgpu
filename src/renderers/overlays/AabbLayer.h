#ifndef _INCLUDE_AABBLAYER_H
#define _INCLUDE_AABBLAYER_H
#include "hatpch.h"

#include "application/DrawCtx.h"
#include "application/Layer.h"
#include "geometry/Aabb.h"
#include "scene/Scene.h"

#include "vk/ctx.h"
#include "vk/deleter.h"
#include "vk/types.h"

namespace hatgpu
{

class AabbLayer : public Layer
{
  public:
    AabbLayer(std::shared_ptr<vk::Ctx> ctx, std::shared_ptr<Scene> scene)
        : Layer("AabbLayer", ctx, scene)
    {}

    void Init() override;
    void OnDetach() override;
    void OnRender(DrawCtx &drawCtx) override;
    void OnImGuiRender() override;

    static const LayerRequirements kRequirements;

  private:
    void createGeometry();
    void uploadGeometry();

    VkPipelineLayout mLayout;
    VkPipeline mPipeline;

    std::vector<glm::vec4> mVertices;
    std::vector<Mesh::IndexType> mIndices;
    vk::AllocatedBuffer mVertexBuffer;
    vk::AllocatedBuffer mIndexBuffer;
};

}  // namespace hatgpu

#endif  //_INCLUDE_AABBLAYER_H
