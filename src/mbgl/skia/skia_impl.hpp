#pragma once

#include <mbgl/gfx/draw_scope.hpp>
#include <mbgl/gfx/drawable_impl.hpp>
#include <mbgl/gfx/drawable_builder.hpp>
#include <mbgl/gfx/dynamic_texture.hpp>
#include <mbgl/gfx/index_buffer.hpp>
#include <mbgl/gfx/index_vector.hpp>
#include <mbgl/gfx/offscreen_texture.hpp>
#include <mbgl/gfx/render_pass.hpp>
#include <mbgl/gfx/renderbuffer.hpp>
#include <mbgl/gfx/texture2d.hpp>
#include <mbgl/gfx/uniform_buffer.hpp>
#include <mbgl/gfx/upload_pass.hpp>
#include <mbgl/gfx/vertex_attribute.hpp>
#include <mbgl/gfx/vertex_vector.hpp>
#include <mbgl/gfx/vertex_buffer.hpp>
#include <mbgl/renderer/layer_group.hpp>
#include <mbgl/skia/context.hpp>
#include <mbgl/skia/skia_fwd.hpp>
#include <mbgl/util/image.hpp>

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMesh.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace mbgl {
namespace skia {

// Owns retained Metal handles when the Skia backend is built against the Metal
// Ganesh path. Both pointers are CFRetain-style references that release on
// destruction. Empty on non-Metal builds.
class MetalGpuHandles {
public:
    MetalGpuHandles() = default;
    MetalGpuHandles(void* device_, void* queue_) noexcept;
    ~MetalGpuHandles();
    MetalGpuHandles(MetalGpuHandles&&) noexcept;
    MetalGpuHandles& operator=(MetalGpuHandles&&) noexcept;
    MetalGpuHandles(const MetalGpuHandles&) = delete;
    MetalGpuHandles& operator=(const MetalGpuHandles&) = delete;

    void* getDevice() const noexcept { return device; }
    void* getQueue() const noexcept { return queue; }

private:
    void* device = nullptr;
    void* queue = nullptr;
};

struct GaneshGpuContext {
    sk_sp<GrDirectContext> context;
    MetalGpuHandles metal;
};

GaneshGpuContext makeDefaultGaneshContext();
bool clipCanvasToTileForTests(SkCanvas&, const mat4&, Size);

// Acquire the next drawable from a CAMetalLayer (bridged void*) and wrap its
// texture in a Skia surface. Skia defers the actual drawable acquisition to
// when the proxy is instantiated (inside flushAndSubmit), and writes the
// CFRetained drawable through the passed-in pointer. The pointer must remain
// valid until after flushAndSubmit completes. Returns nullptr on non-Metal
// builds.
sk_sp<SkSurface> wrapMetalLayerSurface(GrDirectContext* context, void* metalLayer, void** outDrawable);

// Submit a present-drawable command on a fresh MTLCommandBuffer from the
// supplied queue (bridged void*) and release the drawable handle. No-op on
// non-Metal builds.
void presentMetalDrawable(void* metalQueue, void* drawable);

// Release a CFRetained drawable handle without presenting it. Used during
// teardown when a frame was acquired but never submitted.
void releaseMetalDrawable(void* drawable);

// Update the CAMetalLayer's drawableSize. No-op on non-Metal builds.
void updateMetalLayerDrawableSize(void* metalLayer, Size size);

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
    RenderableResource(Size size, GrDirectContext* directContext = nullptr);
    // Layer-backed constructor: subsequent frames pull the next drawable from
    // the supplied CAMetalLayer (bridged void*) and present + commit on the
    // supplied MTLCommandQueue when flush() runs.
    RenderableResource(Size size,
                       GrDirectContext* directContext,
                       void* metalLayer,
                       void* metalQueue);
    ~RenderableResource() override;
    void bind() override;

    SkSurface* getSurface() const;
    const sk_sp<SkSurface>& getSurfaceRef() const;
    SkCanvas* getCanvas() const;
    void flush();

private:
    void ensureSurface();
    void releaseDrawable();

    GrDirectContext* directContext = nullptr;
    void* metalLayer = nullptr;  // CAMetalLayer*, not retained; owned by the GLFW shim.
    void* metalQueue = nullptr;  // id<MTLCommandQueue>, not retained; owned by RendererBackend.
    Size size;

    // Used in offscreen mode; reused across frames.
    sk_sp<SkSurface> staticSurface;

    // Used in layer-backed mode; rebuilt each frame.
    mutable sk_sp<SkSurface> liveSurface;
    mutable void* liveDrawable = nullptr;  // CFRetained id<CAMetalDrawable>
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
    void setImageSnapshot(sk_sp<SkImage> image_);
    void setSnapshotSource(sk_sp<SkSurface> surface_) { snapshotSource = std::move(surface_); }
    const sk_sp<SkImage>& getImage() const;
    const SamplerState& getSamplerState() const { return samplerState; }
    const std::vector<std::uint8_t>& getPixels() const { return pixels; }

    // Lazy SkShader cache. The Skia drawable code repeatedly wraps the same
    // SkImage into a fresh SkShader per drawable per frame; getOrMakeShader()
    // memoizes one combination per texture, invalidated whenever the
    // underlying SkImage changes (set by all the upload paths). For drawables
    // that need a different combination than the current cache, the call
    // builds and stores it (replacing the previous), so adjacent draws using
    // the same combo still reuse.
    sk_sp<SkShader> getOrMakeShader(SkTileMode tileX, SkTileMode tileY, SkSamplingOptions sampling, bool raw) const;
    void invalidateShaderCache() const;

private:
    SamplerState samplerState;
    gfx::TexturePixelType pixelFormat = gfx::TexturePixelType::RGBA;
    gfx::TextureChannelDataType channelType = gfx::TextureChannelDataType::UnsignedByte;
    Size size{0, 0};
    std::shared_ptr<PremultipliedImage> stagedImage;
    std::vector<std::uint8_t> pixels;
    mutable sk_sp<SkImage> skImage;
    sk_sp<SkSurface> snapshotSource;
    bool dirty = false;

    mutable sk_sp<SkShader> cachedShader;
    mutable SkTileMode cachedTileX = SkTileMode::kClamp;
    mutable SkTileMode cachedTileY = SkTileMode::kClamp;
    mutable SkSamplingOptions cachedSampling{};
    mutable bool cachedRaw = false;
    mutable const SkImage* cachedShaderImage = nullptr;
};

class DynamicTexture final : public gfx::DynamicTexture {
public:
    DynamicTexture(gfx::Context& context, Size size, gfx::TexturePixelType pixelType)
        : gfx::DynamicTexture(context, size, pixelType) {}

    void uploadImage(const uint8_t* pixelData, gfx::TextureHandle& texHandle) override;
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
    OffscreenTexture(Size size_, GrDirectContext* directContext = nullptr);
    bool isRenderable() override;
    PremultipliedImage readStillImage() override;
    const gfx::Texture2DPtr& getTexture() override;

private:
    RenderableResource& getSkiaResource() const;
    gfx::Texture2DPtr texture;
};

class RenderPass final : public gfx::RenderPass {
public:
    explicit RenderPass(gfx::Renderable& renderable, const gfx::RenderPassDescriptor& descriptor);
    SkCanvas* getCanvas() const { return canvas; }

protected:
    void pushDebugGroup(const char*) override {}
    void popDebugGroup() override {}

private:
    SkCanvas* canvas = nullptr;
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

// Densely-packed per-vertex data for SkMesh::Make. Filled per draw call from
// the drawable's VertexAttributeArray and any layer/paint state. Lives on the
// drawable as scratch storage so the heap allocation amortizes across frames.
struct MeshVertex {
    float position[2];
    float fillExtrusionZ;
    float fillExtrusionNormal[3];
    float fillExtrusionT;
    float color[4];
    float lineNormal[2];
    float lineWidth[2];
    float lineBlur;
    float lineProgress;
    float lineFloorWidth;
    float circleExtrude[2];
    float circleColor[4];
    float circleStrokeColor[4];
    float circleData[4];
    float rasterTexturePos[2];
    float heatmapWeight[2];
    float heatmapRadius[2];
    float symbolPosOffset[4];
    float symbolData[4];
    float symbolPixelOffset[4];
    float symbolProjectedPos[3];
    float symbolFadeOpacity;
    float symbolOpacity[2];
    float symbolFillColor[4];
    float symbolHaloColor[4];
    float symbolHaloWidth[2];
    float symbolHaloBlur[2];
    float collisionAnchorPos[2];
    float collisionExtrude[2];
    float collisionPlaced[2];
    float collisionShift[2];
    float fillPatternPosA[2];
    float fillPatternPosB[2];
    float fillPatternFrom[4];
    float fillPatternTo[4];
};

class Drawable final : public gfx::Drawable {
public:
    explicit Drawable(std::string name);
    void draw(PaintParameters&) const override;
    void draw(PaintParameters&, const gfx::UniformBufferArray* layerUniforms) const;
    void updateVertexAttributes(gfx::VertexAttributeArrayPtr,
                                std::size_t vertexCount,
                                gfx::DrawMode,
                                gfx::IndexVectorBasePtr,
                                const SegmentBase* segments,
                                std::size_t segmentCount) override;
    void setVertices(std::vector<std::uint8_t>&&, std::size_t, gfx::AttributeDataType) override;
    void setIndexData(gfx::IndexVectorBasePtr, std::vector<UniqueDrawSegment>) override;
    void setVertexAttrId(std::size_t id) { positionAttributeId = id; }
    std::vector<gfx::VertexAttribute::float2> readPackedPositionsForTests() const;
    const gfx::UniformBufferArray& getUniformBuffers() const override;
    gfx::UniformBufferArray& mutableUniformBuffers() override;

private:
    gfx::UniqueUniformBufferArray uniformBuffers;
    gfx::IndexVectorBasePtr sharedIndexes;
    std::vector<UniqueDrawSegment> segments;
    std::vector<std::uint8_t> vertices;
    std::size_t vertexCount = 0;
    gfx::AttributeDataType vertexDataType = gfx::AttributeDataType::Invalid;
    std::size_t positionAttributeId = 0;

    // Per-draw scratch storage. Hoisted onto the drawable so the underlying
    // heap allocations are reused across frames instead of reallocated.
    mutable std::vector<MeshVertex> meshVertexScratch;
    mutable std::vector<std::uint16_t> clippedIndexScratch;
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
