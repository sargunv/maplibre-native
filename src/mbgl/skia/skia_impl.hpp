#pragma once

#include <mbgl/gfx/draw_scope.hpp>
#include <mbgl/gfx/drawable_impl.hpp>
#include <mbgl/gfx/drawable_builder.hpp>
#include <mbgl/gfx/dynamic_texture.hpp>
#include <mbgl/gfx/index_buffer.hpp>
#include <mbgl/gfx/offscreen_texture.hpp>
#include <mbgl/gfx/render_pass.hpp>
#include <mbgl/gfx/renderbuffer.hpp>
#include <mbgl/gfx/texture2d.hpp>
#include <mbgl/gfx/uniform_buffer.hpp>
#include <mbgl/gfx/upload_pass.hpp>
#include <mbgl/gfx/vertex_attribute.hpp>
#include <mbgl/gfx/vertex_buffer.hpp>
#include <mbgl/renderer/layer_group.hpp>
#include <mbgl/skia/context.hpp>
#include <mbgl/util/image.hpp>

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkMesh.h>
#include <include/core/SkSurface.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mbgl {
namespace skia {

class BufferResource final : public gfx::VertexBufferResource, public gfx::IndexBufferResource {
public:
    BufferResource(const void* data, std::size_t size) { update(data, size); }

    void update(const void* data, std::size_t size) {
        bytes.resize(size);
        if (data && size > 0) {
            std::memcpy(bytes.data(), data, size);
        }
    }

private:
    std::vector<std::uint8_t> bytes;
};

class RenderbufferResource final : public gfx::RenderbufferResource {};
class DrawScopeResource final : public gfx::DrawScopeResource {};

class RenderableResource final : public gfx::RenderableResource {
public:
    void bind() override {}
};

class Texture2D final : public gfx::Texture2D {
public:
    gfx::Texture2D& setSamplerConfiguration(const SamplerState& samplerState_) noexcept override;
    gfx::Texture2D& setFormat(gfx::TexturePixelType pixelFormat_,
                              gfx::TextureChannelDataType channelType_) noexcept override;
    gfx::Texture2D& setSize(Size size_) noexcept override;
    gfx::Texture2D& setImage(std::shared_ptr<PremultipliedImage> image_) noexcept override;
    gfx::TexturePixelType getFormat() const noexcept override;
    Size getSize() const noexcept override;
    size_t getDataSize() const noexcept override;
    size_t getPixelStride() const noexcept override;
    size_t numChannels() const noexcept override;
    void create() override;
    void upload(const void* pixelData, const Size& size_) override;
    void uploadSubRegion(const void* pixelData, const Size& size_, uint16_t xOffset, uint16_t yOffset) override;
    void upload() override;
    bool needsUpload() const noexcept override;

private:
    SamplerState samplerState;
    gfx::TexturePixelType pixelFormat = gfx::TexturePixelType::RGBA;
    gfx::TextureChannelDataType channelType = gfx::TextureChannelDataType::UnsignedByte;
    Size size{0, 0};
    std::shared_ptr<PremultipliedImage> image;
    std::vector<std::uint8_t> pixels;
    bool dirty = false;
};

class UniformBuffer final : public gfx::UniformBuffer {
public:
    UniformBuffer(const void* data, std::size_t size_);
    void update(const void* data, std::size_t dataSize) override;
    const std::vector<std::uint8_t>& data() const { return bytes; }

private:
    std::vector<std::uint8_t> bytes;
};

class UniformBufferArray final : public gfx::UniformBufferArray {
public:
    void bind(gfx::RenderPass&) override {}

protected:
    std::unique_ptr<gfx::UniformBuffer> copy(const gfx::UniformBuffer& uniformBuffer) override;
};

class OffscreenTexture final : public gfx::OffscreenTexture {
public:
    explicit OffscreenTexture(Size size_);
    bool isRenderable() override;
    PremultipliedImage readStillImage() override;
    const gfx::Texture2DPtr& getTexture() override;

private:
    gfx::Texture2DPtr texture;
};

class RenderPass final : public gfx::RenderPass {
protected:
    void pushDebugGroup(const char*) override {}
    void popDebugGroup() override {}
};

class UploadPass final : public gfx::UploadPass {
public:
    explicit UploadPass(Context& context_);
    gfx::Context& getContext() override;
    const gfx::Context& getContext() const override;
    gfx::AttributeBindingArray buildAttributeBindings(
        std::size_t,
        gfx::AttributeDataType,
        std::size_t,
        const std::vector<std::uint8_t>&,
        const gfx::VertexAttributeArray&,
        const gfx::VertexAttributeArray&,
        gfx::BufferUsageType,
        std::optional<std::chrono::duration<double>>,
        std::vector<std::unique_ptr<gfx::VertexBufferResource>>&) override;

protected:
    std::unique_ptr<gfx::VertexBufferResource> createVertexBufferResource(const void* data,
                                                                          std::size_t size,
                                                                          gfx::BufferUsageType,
                                                                          bool persistent = false) override;
    void updateVertexBufferResource(gfx::VertexBufferResource&, const void* data, std::size_t size) override;

public:
    std::unique_ptr<gfx::IndexBufferResource> createIndexBufferResource(const void* data,
                                                                        std::size_t size,
                                                                        gfx::BufferUsageType,
                                                                        bool persistent = false) override;
    void updateIndexBufferResource(gfx::IndexBufferResource&, const void* data, std::size_t size) override;

protected:
    void pushDebugGroup(const char*) override {}
    void popDebugGroup() override {}

private:
    Context& context;
};

class CommandEncoder final : public gfx::CommandEncoder {
public:
    explicit CommandEncoder(Context& context_);
    std::unique_ptr<gfx::UploadPass> createUploadPass(const char* name, gfx::Renderable&) override;
    std::unique_ptr<gfx::RenderPass> createRenderPass(const char* name, const gfx::RenderPassDescriptor&) override;
    void present(gfx::Renderable&) override;

protected:
    void pushDebugGroup(const char*) override {}
    void popDebugGroup() override {}

private:
    Context& context;
};

class Drawable final : public gfx::Drawable {
public:
    explicit Drawable(std::string name);
    void draw(PaintParameters&) const override;
    void updateVertexAttributes(gfx::VertexAttributeArrayPtr,
                                std::size_t vertexCount,
                                gfx::DrawMode,
                                gfx::IndexVectorBasePtr,
                                const SegmentBase* segments,
                                std::size_t segmentCount) override;
    void setVertices(std::vector<std::uint8_t>&&, std::size_t, gfx::AttributeDataType) override;
    void setIndexData(gfx::IndexVectorBasePtr, std::vector<UniqueDrawSegment>) override;
    const gfx::UniformBufferArray& getUniformBuffers() const override;
    gfx::UniformBufferArray& mutableUniformBuffers() override;

private:
    gfx::UniqueUniformBufferArray uniformBuffers;
    gfx::IndexVectorBasePtr sharedIndexes;
    std::vector<UniqueDrawSegment> segments;
    std::vector<std::uint8_t> vertices;
};

class DrawableBuilder final : public gfx::DrawableBuilder {
public:
    explicit DrawableBuilder(std::string name);
    std::unique_ptr<gfx::Drawable::DrawSegment> createSegment(gfx::DrawMode, SegmentBase&&) override;

protected:
    gfx::UniqueDrawable createDrawable() const override;
    void init() override;
};

class TileLayerGroup final : public mbgl::TileLayerGroup {
public:
    TileLayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name);
    void render(RenderOrchestrator&, PaintParameters&) override;
    const gfx::UniformBufferArray& getUniformBuffers() const override;
    gfx::UniformBufferArray& mutableUniformBuffers() override;

private:
    gfx::UniqueUniformBufferArray uniformBuffers;
};

class LayerGroup final : public mbgl::LayerGroup {
public:
    LayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name);
    void render(RenderOrchestrator&, PaintParameters&) override;
    const gfx::UniformBufferArray& getUniformBuffers() const override;
    gfx::UniformBufferArray& mutableUniformBuffers() override;

private:
    gfx::UniqueUniformBufferArray uniformBuffers;
};

} // namespace skia
} // namespace mbgl
