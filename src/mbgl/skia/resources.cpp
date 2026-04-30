#include "skia_impl.hpp"

#include <mbgl/util/size.hpp>

#include <algorithm>

namespace mbgl {
namespace skia {

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
    image = std::move(image_);
    dirty = static_cast<bool>(image);
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
    dirty = false;
}

void Texture2D::upload(const void* pixelData, const Size& size_) {
    size = size_;
    const auto byteCount = getDataSize();
    pixels.resize(byteCount);
    if (pixelData && byteCount > 0) {
        std::memcpy(pixels.data(), pixelData, byteCount);
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
}

void Texture2D::upload() {
    if (image) {
        upload(image->data ? image->data.get() : nullptr, image->size);
    }
    dirty = false;
}

bool Texture2D::needsUpload() const noexcept {
    return dirty;
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

OffscreenTexture::OffscreenTexture(Size size_)
    : gfx::OffscreenTexture(size_, std::make_unique<RenderableResource>()),
      texture(std::make_shared<Texture2D>()) {
    texture->setSize(size_).create();
}

bool OffscreenTexture::isRenderable() {
    return true;
}

PremultipliedImage OffscreenTexture::readStillImage() {
    return PremultipliedImage(size);
}

const gfx::Texture2DPtr& OffscreenTexture::getTexture() {
    return texture;
}

} // namespace skia
} // namespace mbgl
