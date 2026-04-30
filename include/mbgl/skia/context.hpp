#pragma once

#include <mbgl/gfx/context.hpp>
#include <mbgl/skia/renderer_backend.hpp>

namespace mbgl {
namespace skia {

class Context final : public gfx::Context {
public:
    explicit Context(RendererBackend& backend_);
    ~Context() override;

    RendererBackend& getBackend() { return backend; }
    const RendererBackend& getBackend() const { return backend; }

    void beginFrame() override;
    void endFrame() override;
    void performCleanup() override;
    void reduceMemoryUsage() override;

    std::unique_ptr<gfx::OffscreenTexture> createOffscreenTexture(Size, gfx::TextureChannelDataType) override;
    std::unique_ptr<gfx::CommandEncoder> createCommandEncoder() override;
    gfx::VertexAttributeArrayPtr createVertexAttributeArray() const override;
    gfx::UniqueDrawableBuilder createDrawableBuilder(std::string name) override;
    gfx::UniformBufferPtr createUniformBuffer(const void* data,
                                              std::size_t size,
                                              bool persistent = false,
                                              bool ssbo = false) override;
    gfx::UniqueUniformBufferArray createLayerUniformBufferArray() override;
    gfx::ShaderProgramBasePtr getGenericShader(gfx::ShaderRegistry&, const std::string& name) override;
    TileLayerGroupPtr createTileLayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name) override;
    LayerGroupPtr createLayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name) override;
    gfx::Texture2DPtr createTexture2D() override;
    gfx::DynamicTexturePtr createDynamicTexture(Size size, gfx::TexturePixelType pixelType) override;
    RenderTargetPtr createRenderTarget(const Size size, const gfx::TextureChannelDataType type) override;

    void resetState(gfx::DepthMode, gfx::ColorMode) override;
    void setDirtyState() override;
    void clearStencilBuffer(int32_t) override;

    bool emplaceOrUpdateUniformBuffer(gfx::UniformBufferPtr&,
                                      const void* data,
                                      std::size_t size,
                                      bool persistent = false) override;
    const gfx::UniformBufferArray& getGlobalUniformBuffers() const override;
    gfx::UniformBufferArray& mutableGlobalUniformBuffers() override;
    void bindGlobalUniformBuffers(gfx::RenderPass&) const noexcept override;
    void unbindGlobalUniformBuffers(gfx::RenderPass&) const noexcept override;

#ifndef NDEBUG
    void visualizeStencilBuffer() override;
    void visualizeDepthBuffer(float depthRangeSize) override;
#endif

protected:
    std::unique_ptr<gfx::RenderbufferResource> createRenderbufferResource(gfx::RenderbufferPixelType, Size) override;
    std::unique_ptr<gfx::DrawScopeResource> createDrawScopeResource() override;

private:
    RendererBackend& backend;
    gfx::UniqueUniformBufferArray globalUniformBuffers;
};

} // namespace skia
} // namespace mbgl
