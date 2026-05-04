#include "skia_impl.hpp"

#include <mbgl/util/image.hpp>
#include <mbgl/util/size.hpp>

#include <include/gpu/ganesh/GrDirectContext.h>
#include <include/gpu/ganesh/SkSurfaceGanesh.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace mbgl {
namespace skia {

namespace {

SkImageInfo makeImageInfo(Size size, gfx::TexturePixelType pixelFormat, gfx::TextureChannelDataType channelType) {
    const auto width = std::max(1, static_cast<int>(size.width));
    const auto height = std::max(1, static_cast<int>(size.height));

    if (pixelFormat == gfx::TexturePixelType::Alpha || pixelFormat == gfx::TexturePixelType::Luminance) {
        return SkImageInfo::Make(width, height, kAlpha_8_SkColorType, kPremul_SkAlphaType);
    }

    const auto colorType = channelType == gfx::TextureChannelDataType::HalfFloat
                               ? kRGBA_F16_SkColorType
                               : (channelType == gfx::TextureChannelDataType::Float ? kRGBA_F32_SkColorType
                                                                                    : kRGBA_8888_SkColorType);
    return SkImageInfo::Make(width, height, colorType, kPremul_SkAlphaType);
}

SkColor4f toSkColor(const Color& color) {
    return {color.r, color.g, color.b, color.a};
}

} // namespace

RenderableResource::RenderableResource(Size size_, GrDirectContext* directContext_)
    : directContext(directContext_), size(size_) {
    const auto info = makeImageInfo(size, gfx::TexturePixelType::RGBA, gfx::TextureChannelDataType::UnsignedByte);
    if (directContext) {
        staticSurface = SkSurfaces::RenderTarget(
            directContext, skgpu::Budgeted::kNo, info, 0, kTopLeft_GrSurfaceOrigin, nullptr);
    }
    if (!staticSurface) {
        staticSurface = SkSurfaces::Raster(info);
    }
}

RenderableResource::RenderableResource(Size size_,
                                       GrDirectContext* directContext_,
                                       void* metalLayer_,
                                       void* metalQueue_)
    : directContext(directContext_),
      metalLayer(metalLayer_),
      metalQueue(metalQueue_),
      size(size_) {
    updateMetalLayerDrawableSize(metalLayer, size);
}

RenderableResource::~RenderableResource() {
    releaseDrawable();
}

void RenderableResource::bind() {
    ensureSurface();
}

SkSurface* RenderableResource::getSurface() const {
    if (metalLayer) {
        const_cast<RenderableResource*>(this)->ensureSurface();
        return liveSurface.get();
    }
    return staticSurface.get();
}

const sk_sp<SkSurface>& RenderableResource::getSurfaceRef() const {
    if (metalLayer) {
        const_cast<RenderableResource*>(this)->ensureSurface();
        return liveSurface;
    }
    return staticSurface;
}

SkCanvas* RenderableResource::getCanvas() const {
    auto* surface = getSurface();
    return surface ? surface->getCanvas() : nullptr;
}

void RenderableResource::flush() {
    if (metalLayer) {
        ensureSurface();
        if (liveSurface && directContext) {
            // Optional debug capture: when MLN_SKIA_FRAME_DUMP is set, snapshot
            // the live SkSurface to a PNG before presenting. This is the only
            // way to see what the live CAMetalLayer path actually drew without
            // relying on screen capture; the headless render-test path uses a
            // different RenderableResource constructor and won't reproduce
            // bugs that live only in the layer-backed surface.
            if (const char* dumpPath = std::getenv("MLN_SKIA_FRAME_DUMP")) {
                // Capture a single frame from the live (CAMetalLayer-backed)
                // path so we can inspect what was actually drawn without screen
                // capture. The headless RenderableResource constructor uses a
                // different surface, so render-test artifacts won't reproduce
                // bugs that live only in the layer-backed path. Default frame
                // is 30 (long enough for tiles to load); override via
                // MLN_SKIA_FRAME_DUMP_FRAME.
                static int frameNum = 0;
                static bool dumped = false;
                ++frameNum;
                int targetFrame = 30;
                if (const char* envFrame = std::getenv("MLN_SKIA_FRAME_DUMP_FRAME")) {
                    targetFrame = std::atoi(envFrame);
                }
                if (!dumped && frameNum == targetFrame) {
                    dumped = true;
                    skgpu::ganesh::FlushAndSubmit(liveSurface);
                    const auto width = liveSurface->width();
                    const auto height = liveSurface->height();
                    PremultipliedImage image(Size{static_cast<uint32_t>(width), static_cast<uint32_t>(height)});
                    const auto info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
                    if (image.valid() &&
                        liveSurface->readPixels(info, image.data.get(), image.stride(), 0, 0)) {
                        auto encoded = encodePNG(image);
                        std::ofstream out(dumpPath, std::ios::binary);
                        out.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
                    }
                }
            }
            skgpu::ganesh::FlushAndSubmit(liveSurface);
        }
        // FlushAndSubmit instantiates the lazy proxy, which is when Skia writes
        // the CFRetained drawable handle into liveDrawable via the pointer we
        // gave WrapCAMetalLayer.
        if (liveDrawable) {
            void* drawable = liveDrawable;
            liveDrawable = nullptr;
            presentMetalDrawable(metalQueue, drawable);
        }
        // Drop the per-frame surface so the next frame acquires a fresh drawable.
        liveSurface.reset();
        return;
    }
    skgpu::ganesh::FlushAndSubmit(staticSurface);
}

void RenderableResource::ensureSurface() {
    if (!metalLayer) return;
    if (liveSurface) return;
    // Pass a pointer to the resource-owned slot. Skia writes the drawable into
    // it during flushAndSubmit, not during this call.
    auto surface = wrapMetalLayerSurface(directContext, metalLayer, &liveDrawable);
    if (surface) {
        liveSurface = std::move(surface);
    }
}

void RenderableResource::releaseDrawable() {
    if (liveDrawable) {
        releaseMetalDrawable(liveDrawable);
        liveDrawable = nullptr;
    }
    liveSurface.reset();
}

gfx::Texture2D& Texture2D::setSamplerConfiguration(const SamplerState& samplerState_) noexcept {
    samplerState = samplerState_;
    return *this;
}

gfx::Texture2D& Texture2D::setFormat(gfx::TexturePixelType pixelFormat_,
                                     gfx::TextureChannelDataType channelType_) noexcept {
    pixelFormat = pixelFormat_;
    channelType = channelType_;
    return *this;
}

gfx::Texture2D& Texture2D::setSize(Size size_) noexcept {
    size = size_;
    return *this;
}

gfx::Texture2D& Texture2D::setImage(std::shared_ptr<PremultipliedImage> image_) noexcept {
    stagedImage = std::move(image_);
    dirty = static_cast<bool>(stagedImage);
    return *this;
}

gfx::TexturePixelType Texture2D::getFormat() const noexcept {
    return pixelFormat;
}

Size Texture2D::getSize() const noexcept {
    return size;
}

size_t Texture2D::getDataSize() const noexcept {
    return static_cast<size_t>(size.width) * static_cast<size_t>(size.height) * getPixelStride();
}

size_t Texture2D::getPixelStride() const noexcept {
    const auto channelBytes = channelType == gfx::TextureChannelDataType::UnsignedByte
                                  ? size_t{1}
                                  : (channelType == gfx::TextureChannelDataType::HalfFloat ? size_t{2} : size_t{4});
    return numChannels() * channelBytes;
}

size_t Texture2D::numChannels() const noexcept {
    switch (pixelFormat) {
        case gfx::TexturePixelType::RGBA:
            return 4;
        case gfx::TexturePixelType::Alpha:
        case gfx::TexturePixelType::Stencil:
        case gfx::TexturePixelType::Depth:
        case gfx::TexturePixelType::Luminance:
            return 1;
    }
    return 4;
}

void Texture2D::create() {
    snapshotSource.reset();
    pixels.assign(getDataSize(), 0);
    if (!pixels.empty()) {
        SkPixmap pixmap(makeImageInfo(size, pixelFormat, channelType), pixels.data(), size.width * getPixelStride());
        skImage = SkImages::RasterFromPixmapCopy(pixmap);
    } else {
        skImage.reset();
    }
    dirty = false;
}

void Texture2D::upload(const void* pixelData, const Size& size_) {
    snapshotSource.reset();
    size = size_;
    const auto byteCount = getDataSize();
    pixels.resize(byteCount);
    if (pixelData && byteCount > 0) {
        std::memcpy(pixels.data(), pixelData, byteCount);
    }
    if (!pixels.empty()) {
        SkPixmap pixmap(makeImageInfo(size, pixelFormat, channelType), pixels.data(), size.width * getPixelStride());
        skImage = SkImages::RasterFromPixmapCopy(pixmap);
    } else {
        skImage.reset();
    }
    dirty = false;
}

void Texture2D::uploadSubRegion(const void* pixelData, const Size& subSize, uint16_t xOffset, uint16_t yOffset) {
    if (!pixelData) {
        return;
    }

    if (pixels.empty()) {
        pixels.assign(getDataSize(), 0);
    }

    const auto stride = getPixelStride();
    const auto rowBytes = static_cast<size_t>(subSize.width) * stride;
    const auto source = static_cast<const std::uint8_t*>(pixelData);
    for (uint32_t row = 0; row < subSize.height; ++row) {
        const auto dstOffset = ((static_cast<size_t>(yOffset) + row) * size.width + xOffset) * stride;
        if (dstOffset + rowBytes <= pixels.size()) {
            std::memcpy(pixels.data() + dstOffset, source + row * rowBytes, rowBytes);
        }
    }
    if (!pixels.empty()) {
        SkPixmap pixmap(makeImageInfo(size, pixelFormat, channelType), pixels.data(), size.width * getPixelStride());
        skImage = SkImages::RasterFromPixmapCopy(pixmap);
    }
}

void Texture2D::upload() {
    if (stagedImage) {
        upload(stagedImage->data ? stagedImage->data.get() : nullptr, stagedImage->size);
    }
    dirty = false;
}

bool Texture2D::needsUpload() const noexcept {
    return dirty;
}

void Texture2D::setImageSnapshot(sk_sp<SkImage> image_) {
    snapshotSource.reset();
    skImage = std::move(image_);
    if (skImage) {
        size = {static_cast<uint32_t>(skImage->width()), static_cast<uint32_t>(skImage->height())};
    }
    dirty = false;
}

const sk_sp<SkImage>& Texture2D::getImage() const {
    if (snapshotSource) {
        skImage = snapshotSource->makeImageSnapshot();
    }
    return skImage;
}

void DynamicTexture::uploadImage(const uint8_t* pixelData, gfx::TextureHandle& texHandle) {
    const auto& rect = texHandle.getRectangle();
    static_cast<Texture2D&>(*texture).uploadSubRegion(pixelData, {rect.w, rect.h}, rect.x, rect.y);
    gfx::DynamicTexture::uploadImage(pixelData, texHandle);
}

UniformBuffer::UniformBuffer(const void* data, std::size_t size_)
    : gfx::UniformBuffer(size_) {
    update(data, size_);
}

void UniformBuffer::update(const void* data, std::size_t dataSize) {
    size = dataSize;
    bytes.resize(dataSize);
    if (data && dataSize > 0) {
        std::memcpy(bytes.data(), data, dataSize);
    }
}

std::unique_ptr<gfx::UniformBuffer> UniformBufferArray::copy(const gfx::UniformBuffer& uniformBuffer) {
    return std::make_unique<UniformBuffer>(nullptr, uniformBuffer.getSize());
}

OffscreenTexture::OffscreenTexture(Size size_, GrDirectContext* directContext)
    : gfx::OffscreenTexture(size_, std::make_unique<RenderableResource>(size_, directContext)),
      texture(std::make_shared<Texture2D>()) {
    texture->setSize(size_).create();
}

bool OffscreenTexture::isRenderable() {
    return true;
}

PremultipliedImage OffscreenTexture::readStillImage() {
    PremultipliedImage image(size);
    auto& resource = getSkiaResource();
    const auto info = makeImageInfo(size, gfx::TexturePixelType::RGBA, gfx::TextureChannelDataType::UnsignedByte);
    if (resource.getSurface() && image.valid()) {
        resource.getSurface()->readPixels(info, image.data.get(), image.stride(), 0, 0);
    }
    return image;
}

const gfx::Texture2DPtr& OffscreenTexture::getTexture() {
    auto& resource = getSkiaResource();
    if (auto* surface = resource.getSurface()) {
        static_cast<Texture2D&>(*texture).setImageSnapshot(surface->makeImageSnapshot());
        static_cast<Texture2D&>(*texture).setSnapshotSource(resource.getSurfaceRef());
    }
    return texture;
}

RenderableResource& OffscreenTexture::getSkiaResource() const {
    return getResource<RenderableResource>();
}

RenderPass::RenderPass(gfx::Renderable& renderable, const gfx::RenderPassDescriptor& descriptor) {
    auto& resource = renderable.getResource<RenderableResource>();
    canvas = resource.getCanvas();
    if (canvas && descriptor.clearColor) {
        canvas->clear(toSkColor(*descriptor.clearColor));
    }
}

} // namespace skia
} // namespace mbgl
