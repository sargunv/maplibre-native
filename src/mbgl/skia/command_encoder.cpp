#include "skia_impl.hpp"

namespace mbgl {
namespace skia {

CommandEncoder::CommandEncoder(Context& context_)
    : context(context_) {}

std::unique_ptr<gfx::UploadPass> CommandEncoder::createUploadPass(const char*, gfx::Renderable&) {
    return std::make_unique<UploadPass>(context);
}

std::unique_ptr<gfx::RenderPass> CommandEncoder::createRenderPass(const char*,
                                                                  const gfx::RenderPassDescriptor& descriptor) {
    return std::make_unique<RenderPass>(descriptor.renderable, descriptor);
}

void CommandEncoder::present(gfx::Renderable& renderable) {
    renderable.getResource<RenderableResource>().flush();
}

UploadPass::UploadPass(Context& context_)
    : context(context_) {}

gfx::Context& UploadPass::getContext() {
    return context;
}

const gfx::Context& UploadPass::getContext() const {
    return context;
}

gfx::AttributeBindingArray UploadPass::buildAttributeBindings(
    std::size_t,
    gfx::AttributeDataType,
    std::size_t,
    const std::vector<std::uint8_t>&,
    const gfx::VertexAttributeArray& defaults,
    const gfx::VertexAttributeArray& overrides,
    gfx::BufferUsageType,
    std::optional<std::chrono::duration<double>>,
    std::vector<std::unique_ptr<gfx::VertexBufferResource>>&) {
    gfx::AttributeBindingArray bindings;
    bindings.resize(defaults.allocatedSize());
    overrides.visitAttributes([&](const gfx::VertexAttribute& attr) {
        if (attr.getIndex() >= 0 && static_cast<std::size_t>(attr.getIndex()) >= bindings.size()) {
            bindings.resize(static_cast<std::size_t>(attr.getIndex()) + 1);
        }
    });
    return bindings;
}

std::unique_ptr<gfx::VertexBufferResource> UploadPass::createVertexBufferResource(const void* data,
                                                                                  std::size_t size,
                                                                                  gfx::BufferUsageType,
                                                                                  bool) {
    return std::unique_ptr<gfx::VertexBufferResource>(new BufferResource(data, size));
}

void UploadPass::updateVertexBufferResource(gfx::VertexBufferResource& resource, const void* data, std::size_t size) {
    static_cast<BufferResource&>(resource).update(data, size);
}

std::unique_ptr<gfx::IndexBufferResource> UploadPass::createIndexBufferResource(const void* data,
                                                                                std::size_t size,
                                                                                gfx::BufferUsageType,
                                                                                bool) {
    return std::unique_ptr<gfx::IndexBufferResource>(new BufferResource(data, size));
}

void UploadPass::updateIndexBufferResource(gfx::IndexBufferResource& resource, const void* data, std::size_t size) {
    static_cast<BufferResource&>(resource).update(data, size);
}

} // namespace skia
} // namespace mbgl
