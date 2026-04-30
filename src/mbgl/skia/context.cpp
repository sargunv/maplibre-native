#include <mbgl/skia/context.hpp>

#include <mbgl/gfx/color_mode.hpp>
#include <mbgl/gfx/depth_mode.hpp>
#include <mbgl/gfx/dynamic_texture.hpp>
#include <mbgl/gfx/shader_registry.hpp>
#include <mbgl/renderer/render_target.hpp>
#include <mbgl/shaders/shader_program_base.hpp>
#include "skia_impl.hpp"

namespace mbgl {
namespace skia {

Context::Context(RendererBackend& backend_)
    : gfx::Context(gfx::Context::minimumRequiredVertexBindingCount),
      backend(backend_),
      globalUniformBuffers(std::make_unique<UniformBufferArray>()) {}

Context::~Context() = default;

void Context::beginFrame() {}
void Context::endFrame() {}
void Context::performCleanup() {}
void Context::reduceMemoryUsage() {}

std::unique_ptr<gfx::OffscreenTexture> Context::createOffscreenTexture(Size size, gfx::TextureChannelDataType) {
    return std::make_unique<OffscreenTexture>(size, backend.getDirectContext());
}

std::unique_ptr<gfx::CommandEncoder> Context::createCommandEncoder() {
    return std::make_unique<CommandEncoder>(*this);
}

gfx::VertexAttributeArrayPtr Context::createVertexAttributeArray() const {
    return std::make_shared<gfx::VertexAttributeArray>();
}

gfx::UniqueDrawableBuilder Context::createDrawableBuilder(std::string name) {
    return std::make_unique<DrawableBuilder>(std::move(name));
}

gfx::UniformBufferPtr Context::createUniformBuffer(const void* data, std::size_t size, bool, bool) {
    return std::make_shared<UniformBuffer>(data, size);
}

gfx::UniqueUniformBufferArray Context::createLayerUniformBufferArray() {
    return std::make_unique<UniformBufferArray>();
}

gfx::ShaderProgramBasePtr Context::getGenericShader(gfx::ShaderRegistry& registry, const std::string& name) {
    const auto shaderGroup = registry.getShaderGroup(name);
    return shaderGroup ? std::static_pointer_cast<gfx::ShaderProgramBase>(shaderGroup->getOrCreateShader(*this, {}))
                       : gfx::ShaderProgramBasePtr{};
}

TileLayerGroupPtr Context::createTileLayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name) {
    return std::make_shared<TileLayerGroup>(layerIndex, initialCapacity, std::move(name));
}

LayerGroupPtr Context::createLayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name) {
    return std::make_shared<LayerGroup>(layerIndex, initialCapacity, std::move(name));
}

gfx::Texture2DPtr Context::createTexture2D() {
    return std::make_shared<Texture2D>();
}

gfx::DynamicTexturePtr Context::createDynamicTexture(Size size, gfx::TexturePixelType pixelType) {
    return std::make_shared<gfx::DynamicTexture>(*this, size, pixelType);
}

RenderTargetPtr Context::createRenderTarget(const Size size, const gfx::TextureChannelDataType type) {
    return std::make_shared<RenderTarget>(*this, size, type);
}

void Context::resetState(gfx::DepthMode, gfx::ColorMode) {}
void Context::setDirtyState() {}
void Context::clearStencilBuffer(int32_t) {}

bool Context::emplaceOrUpdateUniformBuffer(gfx::UniformBufferPtr& ptr,
                                           const void* data,
                                           std::size_t size,
                                           bool persistent) {
    if (!ptr) {
        ptr = createUniformBuffer(data, size, persistent);
        return true;
    }
    ptr->update(data, size);
    return false;
}

const gfx::UniformBufferArray& Context::getGlobalUniformBuffers() const {
    return *globalUniformBuffers;
}

gfx::UniformBufferArray& Context::mutableGlobalUniformBuffers() {
    return *globalUniformBuffers;
}

void Context::bindGlobalUniformBuffers(gfx::RenderPass&) const noexcept {}
void Context::unbindGlobalUniformBuffers(gfx::RenderPass&) const noexcept {}

#ifndef NDEBUG
void Context::visualizeStencilBuffer() {}
void Context::visualizeDepthBuffer(float) {}
#endif

std::unique_ptr<gfx::RenderbufferResource> Context::createRenderbufferResource(gfx::RenderbufferPixelType, Size) {
    return std::make_unique<RenderbufferResource>();
}

std::unique_ptr<gfx::DrawScopeResource> Context::createDrawScopeResource() {
    return std::make_unique<DrawScopeResource>();
}

} // namespace skia
} // namespace mbgl
