#include "skia_impl.hpp"

#include <mbgl/util/size.hpp>

#include <include/gpu/ganesh/GrDirectContext.h>
#include <include/gpu/ganesh/SkSurfaceGanesh.h>

#include <algorithm>

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

RenderableResource::RenderableResource(Size size, GrDirectContext* directContext) {
    const auto info = makeImageInfo(size, gfx::TexturePixelType::RGBA, gfx::TextureChannelDataType::UnsignedByte);
    if (directContext) {
        surface = SkSurfaces::RenderTarget(directContext, skgpu::Budgeted::kNo, info, 0, kTopLeft_GrSurfaceOrigin, nullptr);
    }
    if (!surface) {
        surface = SkSurfaces::Raster(info);
    }
}

void RenderableResource::flush() const {
    skgpu::ganesh::FlushAndSubmit(surface);
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
    if (!pixelData || pixels.empty()) {
        return;
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
    skImage = std::move(image_);
    if (skImage) {
        size = {static_cast<uint32_t>(skImage->width()), static_cast<uint32_t>(skImage->height())};
    }
    dirty = false;
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
    if (auto* surface = getSkiaResource().getSurface()) {
        static_cast<Texture2D&>(*texture).setImageSnapshot(surface->makeImageSnapshot());
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
