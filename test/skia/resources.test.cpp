#include <mbgl/test/util.hpp>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/skia/renderer_backend.hpp>
#include <mbgl/util/image.hpp>

#include <mbgl/skia/skia_impl.hpp>

#include <mbgl/gfx/draw_mode.hpp>
#include <mbgl/gfx/vertex_attribute.hpp>
#include <mbgl/gfx/vertex_vector.hpp>
#include <mbgl/util/constants.hpp>

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>

namespace mbgl {
namespace skia {
namespace {

template <typename T>
std::vector<std::uint8_t> rawBytes(const std::vector<T>& values) {
    std::vector<std::uint8_t> bytes(values.size() * sizeof(T));
    std::memcpy(bytes.data(), values.data(), bytes.size());
    return bytes;
}

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

TEST(SkiaResource, OffscreenTextureSnapshotOwnsSurface) {
    auto backend = gfx::HeadlessBackend::Create({2, 2});
    gfx::BackendScope scope{*backend->getRendererBackend()};
    auto& context = backend->getRendererBackend()->getContext();
    gfx::Texture2DPtr texture;

    {
        auto encoder = context.createCommandEncoder();
        auto offscreenTexture = context.createOffscreenTexture({2, 2}, gfx::TextureChannelDataType::UnsignedByte);
        auto renderPass = encoder->createRenderPass("skia offscreen snapshot",
                                                    {*offscreenTexture, Color{0.0f, 1.0f, 0.0f, 1.0f}, {}, {}});
        renderPass.reset();
        texture = offscreenTexture->getTexture();
    }

    auto skiaTexture = std::static_pointer_cast<Texture2D>(texture);
    ASSERT_TRUE(skiaTexture);
    ASSERT_TRUE(skiaTexture->getImage());
    EXPECT_EQ(2, skiaTexture->getImage()->width());
    EXPECT_EQ(2, skiaTexture->getImage()->height());
}

TEST(SkiaResource, VertexAttributePositionPacking) {
    Drawable rawDrawable("raw-position-test");
    rawDrawable.setVertices(rawBytes(std::vector<std::int16_t>{10, 20, -30, 40}), 2, gfx::AttributeDataType::Short2);
    EXPECT_EQ((std::vector<gfx::VertexAttribute::float2>{{10.0f, 20.0f}, {-30.0f, 40.0f}}),
              rawDrawable.readPackedPositionsForTests());

    constexpr std::size_t positionAttributeId = 7;
    auto rawAttributes = std::make_shared<gfx::VertexAttributeArray>();
    const auto& rawPosition = rawAttributes->set(positionAttributeId, -1, gfx::AttributeDataType::Short4, 2);
    rawPosition->setRawData(rawBytes(std::vector<std::int16_t>{1, 2, 99, 99, 3, 4, 88, 88}));

    Drawable attributeDrawable("raw-attribute-position-test");
    attributeDrawable.setVertexAttrId(positionAttributeId);
    attributeDrawable.updateVertexAttributes(rawAttributes, 2, gfx::Triangles(), nullptr, nullptr, 0);
    EXPECT_EQ((std::vector<gfx::VertexAttribute::float2>{{1.0f, 2.0f}, {3.0f, 4.0f}}),
              attributeDrawable.readPackedPositionsForTests());

    struct InterleavedVertex {
        std::uint8_t prefix[2];
        std::int16_t position[2];
        std::uint8_t suffix[2];
    };

    auto sharedVertices = std::make_shared<gfx::VertexVector<InterleavedVertex>>();
    sharedVertices->emplace_back(InterleavedVertex{{0, 0}, {100, 200}, {0, 0}});
    sharedVertices->emplace_back(InterleavedVertex{{0, 0}, {-300, 400}, {0, 0}});
    sharedVertices->emplace_back(InterleavedVertex{{0, 0}, {500, -600}, {0, 0}});

    auto sharedAttributes = std::make_shared<gfx::VertexAttributeArray>();
    const auto& sharedPosition = sharedAttributes->set(positionAttributeId, -1, gfx::AttributeDataType::Short2, 0);
    sharedPosition->setSharedRawData(sharedVertices,
                                     offsetof(InterleavedVertex, position),
                                     1,
                                     sizeof(InterleavedVertex),
                                     gfx::AttributeDataType::Short2);

    Drawable sharedDrawable("shared-position-test");
    sharedDrawable.setVertexAttrId(positionAttributeId);
    sharedDrawable.updateVertexAttributes(sharedAttributes, 2, gfx::Triangles(), nullptr, nullptr, 0);
    EXPECT_EQ((std::vector<gfx::VertexAttribute::float2>{{-300.0f, 400.0f}, {500.0f, -600.0f}}),
              sharedDrawable.readPackedPositionsForTests());
}

TEST(SkiaResource, UniformBufferPackingAndUpdate) {
    struct Payload {
        float matrix[4];
        std::uint32_t flags;
    };

    const Payload initial{{1.0f, 2.0f, 3.0f, 4.0f}, 7};
    UniformBuffer buffer(&initial, sizeof(initial));

    EXPECT_EQ(sizeof(initial), buffer.getSize());
    ASSERT_EQ(sizeof(initial), buffer.data().size());
    EXPECT_EQ(0, std::memcmp(buffer.data().data(), &initial, sizeof(initial)));

    const std::array<std::uint8_t, 3> replacement = {9, 8, 7};
    buffer.update(replacement.data(), replacement.size());
    EXPECT_EQ(replacement.size(), buffer.getSize());
    EXPECT_EQ((std::vector<std::uint8_t>{9, 8, 7}), buffer.data());
}

TEST(SkiaResource, UniformBufferArrayCreateOrUpdate) {
    auto backend = gfx::HeadlessBackend::Create({1, 1});
    gfx::BackendScope scope{*backend->getRendererBackend()};
    auto& context = backend->getRendererBackend()->getContext();
    auto uniformBuffers = context.createLayerUniformBufferArray();

    std::uint32_t value = 0x12345678u;
    constexpr std::size_t uniformId = 0;
    uniformBuffers->createOrUpdate(uniformId, &value, sizeof(value), context);

    auto first = std::static_pointer_cast<UniformBuffer>(uniformBuffers->get(uniformId));
    ASSERT_TRUE(first);
    EXPECT_EQ(sizeof(value), first->getSize());
    EXPECT_EQ(0, std::memcmp(first->data().data(), &value, sizeof(value)));

    value = 0x90abcdefu;
    uniformBuffers->createOrUpdate(uniformId, &value, sizeof(value), context);
    auto second = std::static_pointer_cast<UniformBuffer>(uniformBuffers->get(uniformId));
    EXPECT_EQ(first.get(), second.get());
    EXPECT_EQ(0, std::memcmp(second->data().data(), &value, sizeof(value)));

    const std::array<std::uint8_t, 6> larger = {1, 2, 3, 4, 5, 6};
    uniformBuffers->createOrUpdate(uniformId, larger.data(), larger.size(), context);
    auto third = std::static_pointer_cast<UniformBuffer>(uniformBuffers->get(uniformId));
    EXPECT_NE(second.get(), third.get());
    EXPECT_EQ(larger.size(), third->getSize());
    EXPECT_EQ(std::vector<std::uint8_t>(larger.begin(), larger.end()), third->data());
}

TEST(SkiaResource, TileClipConstrainsCanvasDrawing) {
    auto surface = SkSurfaces::Raster(SkImageInfo::Make(4, 2, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
    ASSERT_TRUE(surface);
    auto* canvas = surface->getCanvas();
    ASSERT_NE(nullptr, canvas);

    canvas->clear(SK_ColorBLUE);

    mat4 matrix = {0.0};
    matrix[0] = 1.0 / util::EXTENT;
    matrix[5] = -2.0 / util::EXTENT;
    matrix[12] = -1.0;
    matrix[13] = 1.0;
    matrix[15] = 1.0;

    canvas->save();
    EXPECT_TRUE(clipCanvasToTileForTests(*canvas, matrix, {4, 2}));

    SkPaint paint;
    paint.setColor(SK_ColorRED);
    canvas->drawRect(SkRect::MakeWH(4.0f, 2.0f), paint);
    canvas->restore();

    std::array<std::uint8_t, 4 * 2 * 4> pixels{};
    ASSERT_TRUE(surface->readPixels(
        SkImageInfo::Make(4, 2, kRGBA_8888_SkColorType, kPremul_SkAlphaType), pixels.data(), 4 * 4, 0, 0));

    EXPECT_EQ((std::array<std::uint8_t, 4>{255, 0, 0, 255}),
              (std::array<std::uint8_t, 4>{pixels[0], pixels[1], pixels[2], pixels[3]}));
    const auto rightPixelOffset = std::size_t{3} * 4;
    EXPECT_EQ((std::array<std::uint8_t, 4>{0, 0, 255, 255}),
              (std::array<std::uint8_t, 4>{pixels[rightPixelOffset + 0],
                                           pixels[rightPixelOffset + 1],
                                           pixels[rightPixelOffset + 2],
                                           pixels[rightPixelOffset + 3]}));

    matrix[0] = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(clipCanvasToTileForTests(*canvas, matrix, {4, 2}));
}

} // namespace
} // namespace skia
} // namespace mbgl
