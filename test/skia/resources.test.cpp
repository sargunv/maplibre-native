#include <mbgl/test/util.hpp>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/skia/renderer_backend.hpp>
#include <mbgl/util/image.hpp>

#include <mbgl/skia/skia_impl.hpp>

#include <array>

namespace mbgl {
namespace skia {
namespace {

TEST(SkiaResource, TextureUploadAndSubRegion) {
    Texture2D texture;
    texture.setSize({2, 2}).setFormat(gfx::TexturePixelType::RGBA, gfx::TextureChannelDataType::UnsignedByte).create();

    EXPECT_FALSE(texture.needsUpload());
    EXPECT_EQ(16u, texture.getDataSize());
    EXPECT_EQ(4u, texture.getPixelStride());
    EXPECT_EQ(4u, texture.numChannels());
    EXPECT_EQ(16u, texture.getPixels().size());

    const std::array<std::uint8_t, 16> pixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255};
    texture.upload(pixels.data(), {2, 2});
    EXPECT_EQ(std::vector<std::uint8_t>(pixels.begin(), pixels.end()), texture.getPixels());
    ASSERT_TRUE(texture.getImage());
    EXPECT_EQ(2, texture.getImage()->width());
    EXPECT_EQ(2, texture.getImage()->height());

    const std::array<std::uint8_t, 4> replacement = {12, 34, 56, 78};
    texture.uploadSubRegion(replacement.data(), {1, 1}, 1, 0);

    auto expected = std::vector<std::uint8_t>(pixels.begin(), pixels.end());
    std::copy(replacement.begin(), replacement.end(), expected.begin() + 4);
    EXPECT_EQ(expected, texture.getPixels());
}

TEST(SkiaResource, OffscreenReadbackAfterClear) {
    auto backend = gfx::HeadlessBackend::Create({2, 2});
    gfx::BackendScope scope{*backend->getRendererBackend()};
    auto& context = backend->getRendererBackend()->getContext();
    auto encoder = context.createCommandEncoder();
    auto offscreenTexture = context.createOffscreenTexture({2, 2}, gfx::TextureChannelDataType::UnsignedByte);

    auto renderPass = encoder->createRenderPass("skia offscreen clear",
                                                {*offscreenTexture, Color{1.0f, 0.0f, 0.0f, 1.0f}, {}, {}});
    renderPass.reset();
    encoder.reset();

    const auto image = offscreenTexture->readStillImage();
    ASSERT_TRUE(image.valid());
    EXPECT_EQ(Size(2, 2), image.size);

    for (std::size_t offset = 0; offset < image.bytes(); offset += 4) {
        EXPECT_EQ(255, image.data[offset + 0]);
        EXPECT_EQ(0, image.data[offset + 1]);
        EXPECT_EQ(0, image.data[offset + 2]);
        EXPECT_EQ(255, image.data[offset + 3]);
    }
}

} // namespace
} // namespace skia
} // namespace mbgl
