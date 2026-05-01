#include "skia_impl.hpp"

#include <mbgl/gfx/hillshade_prepare_drawable_data.hpp>
#include <mbgl/renderer/paint_parameters.hpp>
#include <mbgl/skia/renderer_backend.hpp>
#include <mbgl/shaders/background_layer_ubo.hpp>
#include <mbgl/shaders/circle_layer_ubo.hpp>
#include <mbgl/shaders/collision_layer_ubo.hpp>
#include <mbgl/shaders/color_relief_layer_ubo.hpp>
#include <mbgl/shaders/fill_extrusion_layer_ubo.hpp>
#include <mbgl/shaders/fill_layer_ubo.hpp>
#include <mbgl/shaders/heatmap_layer_ubo.hpp>
#include <mbgl/shaders/heatmap_texture_layer_ubo.hpp>
#include <mbgl/shaders/hillshade_layer_ubo.hpp>
#include <mbgl/shaders/hillshade_prepare_layer_ubo.hpp>
#include <mbgl/shaders/line_layer_ubo.hpp>
#include <mbgl/shaders/raster_layer_ubo.hpp>
#include <mbgl/shaders/shader_defines.hpp>
#include <mbgl/shaders/symbol_layer_ubo.hpp>

#include <include/core/SkBlender.h>
#include <include/core/SkColor.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkMesh.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkString.h>
#include <include/core/SkTileMode.h>
#include <include/gpu/ganesh/SkMeshGanesh.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace mbgl {
namespace skia {

namespace {

struct VertexReader {
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;

    bool read(std::uint16_t index, float& x, float& y) const {
        if (!data || index >= count || stride < sizeof(std::int16_t) * 2) {
            return false;
        }
        std::int16_t xy[2];
        std::memcpy(xy, data + std::size_t(index) * stride, sizeof(xy));
        x = static_cast<float>(xy[0]);
        y = static_cast<float>(xy[1]);
        return true;
    }
};

struct Short4Reader {
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;

    bool read(std::uint16_t index, std::array<std::int16_t, 4>& value) const {
        if (!data || index >= count || stride < sizeof(std::int16_t) * 4) {
            return false;
        }
        std::memcpy(value.data(), data + std::size_t(index) * stride, sizeof(std::int16_t) * 4);
        return true;
    }
};

struct UByte4Reader {
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;

    bool read(std::uint16_t index, std::array<std::uint8_t, 4>& value) const {
        if (!data || index >= count || stride < sizeof(std::uint8_t) * 4) {
            return false;
        }
        std::memcpy(value.data(), data + std::size_t(index) * stride, sizeof(std::uint8_t) * 4);
        return true;
    }
};

struct FloatReader {
    const gfx::VertexAttribute* attribute = nullptr;
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;

    bool read(std::uint16_t index, float& value) const {
        if (data && index < count && stride >= sizeof(float)) {
            std::memcpy(&value, data + std::size_t(index) * stride, sizeof(float));
            return true;
        }
        if (attribute && !attribute->getSharedRawData() && index < attribute->getCount()) {
            if (const auto* item = std::get_if<float>(&attribute->get(index))) {
                value = *item;
                return true;
            }
        }
        return false;
    }
};

struct Float3Reader {
    const gfx::VertexAttribute* attribute = nullptr;
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;

    bool read(std::uint16_t index, std::array<float, 3>& value) const {
        if (data && index < count && stride >= sizeof(float) * 3) {
            std::memcpy(value.data(), data + std::size_t(index) * stride, sizeof(float) * 3);
            return true;
        }
        if (attribute && !attribute->getSharedRawData() && index < attribute->getCount()) {
            if (const auto* item = std::get_if<gfx::VertexAttribute::float3>(&attribute->get(index))) {
                value = *item;
                return true;
            }
        }
        return false;
    }
};

struct UShort4Reader {
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;

    bool read(std::uint16_t index, std::array<float, 4>& value) const {
        if (!data || index >= count || stride < sizeof(std::uint16_t) * 4) {
            return false;
        }
        std::uint16_t packed[4];
        std::memcpy(packed, data + std::size_t(index) * stride, sizeof(packed));
        value = {static_cast<float>(packed[0]),
                 static_cast<float>(packed[1]),
                 static_cast<float>(packed[2]),
                 static_cast<float>(packed[3])};
        return true;
    }
};

struct UShort2Reader {
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;

    bool read(std::uint16_t index, std::array<float, 2>& value) const {
        if (!data || index >= count || stride < sizeof(std::uint16_t) * 2) {
            return false;
        }
        std::uint16_t packed[2];
        std::memcpy(packed, data + std::size_t(index) * stride, sizeof(packed));
        value = {static_cast<float>(packed[0]), static_cast<float>(packed[1])};
        return true;
    }
};

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

struct Float4Reader {
    const gfx::VertexAttribute* attribute = nullptr;
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;
    std::size_t components = 4;

    bool read(std::uint16_t index, std::array<float, 4>& value) const {
        if (data && index < count && stride >= sizeof(float) * components) {
            std::memcpy(value.data(), data + std::size_t(index) * stride, sizeof(float) * components);
            if (components == 2) {
                value[2] = value[0];
                value[3] = value[1];
            }
            return true;
        }
        if (attribute && !attribute->getSharedRawData() && index < attribute->getCount()) {
            if (const auto* item = std::get_if<gfx::VertexAttribute::float4>(&attribute->get(index))) {
                value = *item;
                return true;
            }
            if (const auto* item = std::get_if<gfx::VertexAttribute::float2>(&attribute->get(index))) {
                value = {(*item)[0], (*item)[1], (*item)[0], (*item)[1]};
                return true;
            }
        }
        return false;
    }
};

struct Float2Reader {
    const gfx::VertexAttribute* attribute = nullptr;
    const std::uint8_t* data = nullptr;
    std::size_t count = 0;
    std::size_t stride = 0;
    std::size_t components = 2;

    bool read(std::uint16_t index, std::array<float, 2>& value) const {
        if (data && index < count && stride >= sizeof(float) * components) {
            std::memcpy(value.data(), data + std::size_t(index) * stride, sizeof(float) * components);
            if (components == 1) {
                value[1] = value[0];
            }
            return true;
        }
        if (attribute && !attribute->getSharedRawData() && index < attribute->getCount()) {
            if (const auto* item = std::get_if<gfx::VertexAttribute::float2>(&attribute->get(index))) {
                value = *item;
                return true;
            }
            if (const auto* item = std::get_if<float>(&attribute->get(index))) {
                value = {*item, *item};
                return true;
            }
        }
        return false;
    }
};

const UniformBuffer* getSkiaBuffer(const gfx::UniformBufferArray* uniforms, const std::size_t id) {
    if (!uniforms) {
        return nullptr;
    }
    const auto& buffer = uniforms->get(id);
    return buffer ? static_cast<const UniformBuffer*>(buffer.get()) : nullptr;
}

template <typename T>
const T* getUBO(const gfx::UniformBufferArray* uniforms, const std::size_t id, const std::size_t index = 0) {
    const auto* buffer = getSkiaBuffer(uniforms, id);
    if (!buffer) {
        return nullptr;
    }
    const auto offset = sizeof(T) * index;
    const auto& data = buffer->data();
    if (data.size() < offset + sizeof(T)) {
        return nullptr;
    }
    return reinterpret_cast<const T*>(data.data() + offset);
}

SkColor4f toSkColor(const Color& color, const float opacity = 1.0f) {
    const auto alpha = color.a * opacity;
    return SkColor4f{color.r * alpha, color.g * alpha, color.b * alpha, alpha};
}

SkColor4f toRawSkColor(const Color& color) {
    return SkColor4f{color.r, color.g, color.b, color.a};
}

std::array<float, 4> hillshadeUnpackVector(const Tileset::RasterEncoding encoding) {
    if (encoding == Tileset::RasterEncoding::Terrarium) {
        return {256.0f, 1.0f, 1.0f / 256.0f, 32768.0f};
    }
    return {6553.6f, 25.6f, 0.1f, 10000.0f};
}

std::array<float, 2> unpackFloat(const float packedValue) {
    const auto packedIntValue = static_cast<int>(packedValue);
    const auto v0 = packedIntValue / 256;
    return {static_cast<float>(v0), static_cast<float>(packedIntValue - v0 * 256)};
}

SkColor4f decodeColor(const std::array<float, 2>& encoded) {
    const auto rg = unpackFloat(encoded[0]);
    const auto ba = unpackFloat(encoded[1]);
    return SkColor4f{rg[0] / 255.0f, rg[1] / 255.0f, ba[0] / 255.0f, ba[1] / 255.0f};
}

SkColor4f mixColors(const SkColor4f& from, const SkColor4f& to, const float t) {
    return SkColor4f{from.fR + (to.fR - from.fR) * t,
                     from.fG + (to.fG - from.fG) * t,
                     from.fB + (to.fB - from.fB) * t,
                     from.fA + (to.fA - from.fA) * t};
}

SkColor4f unpackMixColor(const std::array<float, 4>& packedColors, const float t) {
    if (std::ranges::all_of(packedColors, [](const auto value) { return value >= 0.0f && value <= 1.0f; })) {
        return SkColor4f{packedColors[0], packedColors[1], packedColors[2], packedColors[3]};
    }
    return mixColors(decodeColor({packedColors[0], packedColors[1]}), decodeColor({packedColors[2], packedColors[3]}), t);
}

float unpackMixFloat(const std::array<float, 2>& packedValue, const float t) {
    return packedValue[0] + (packedValue[1] - packedValue[0]) * t;
}

SkColor4f premultiply(const SkColor4f& color, const float opacity = 1.0f) {
    const auto alpha = color.fA * opacity;
    return SkColor4f{color.fR * alpha, color.fG * alpha, color.fB * alpha, alpha};
}

std::array<float, 16> identityMatrix() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

SkRect conservativeMeshBounds() {
    constexpr auto size = 1.0e9f;
    return SkRect::MakeLTRB(-size, -size, size, size);
}

std::array<float, 2> linePositionFromPosNormal(const float x, const float y) {
    return {std::floor(x * 0.5f), std::floor(y * 0.5f)};
}

std::array<float, 2> lineNormalFromPosNormal(const float x, const float y) {
    const auto pos = linePositionFromPosNormal(x, y);
    const auto normalX = x - 2.0f * pos[0];
    const auto normalY = y - 2.0f * pos[1];
    return {normalX, normalY * 2.0f - 1.0f};
}

std::array<float, 2> lineWidthPair(const float width, const float gapwidth) {
    constexpr auto antialiasing = 0.5f;
    const auto halfwidth = width * 0.5f;
    const auto gap = gapwidth * 0.5f;
    const auto inset = gap + (gap > 0.0f ? antialiasing : 0.0f);
    const auto outset = gap + halfwidth * (gap > 0.0f ? 2.0f : 1.0f) + (halfwidth == 0.0f ? 0.0f : antialiasing);
    return {outset, inset};
}

std::array<float, 2> lineExtrudePosition(const float posX,
                                         const float posY,
                                         const std::array<std::uint8_t, 4>& data,
                                         const float width,
                                         const float gapwidth,
                                         const float offset,
                                         const float normalY,
                                         const float ratio) {
    constexpr auto lineNormalScale = 1.0f / (127.0f / 2.0f);
    const auto outset = lineWidthPair(width, gapwidth)[0];
    const auto extrudeX = static_cast<float>(data[0]) - 128.0f;
    const auto extrudeY = static_cast<float>(data[1]) - 128.0f;
    const auto direction = std::fmod(static_cast<float>(data[2]), 4.0f) - 1.0f;
    const auto u = 0.5f * direction;
    const auto t = 1.0f - std::abs(u);
    const auto safeRatio = ratio == 0.0f ? 1.0f : ratio;
    const auto offsetX = offset * extrudeX * lineNormalScale * normalY * t + offset * extrudeY * lineNormalScale * normalY * u;
    const auto offsetY = offset * extrudeX * lineNormalScale * normalY * -u + offset * extrudeY * lineNormalScale * normalY * t;
    return {posX + (offsetX + outset * extrudeX * lineNormalScale) / safeRatio,
            posY + (offsetY + outset * extrudeY * lineNormalScale) / safeRatio};
}

std::array<float, 2> modPositive(const std::array<float, 2>& x, const std::array<float, 2>& y) {
    return {x[0] - y[0] * std::floor(x[0] / y[0]),
            x[1] - y[1] * std::floor(x[1] / y[1])};
}

std::array<float, 2> getPatternPos(const std::array<float, 2>& pixelCoordUpper,
                                   const std::array<float, 2>& pixelCoordLower,
                                   const std::array<float, 2>& patternSize,
                                   const float tileRatio,
                                   const std::array<float, 2>& pos) {
    const auto first = modPositive(pixelCoordUpper, patternSize);
    const auto second = modPositive({first[0] * 256.0f, first[1] * 256.0f}, patternSize);
    const auto offset = modPositive({second[0] * 256.0f + pixelCoordLower[0], second[1] * 256.0f + pixelCoordLower[1]}, patternSize);
    return {(tileRatio * pos[0] + offset[0]) / patternSize[0],
            (tileRatio * pos[1] + offset[1]) / patternSize[1]};
}

sk_sp<SkMeshSpecification> solidColorMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, color), SkString("a_color")}};
    const Varying varyings[] = {{Varying::Type::kFloat4, SkString("color")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.color = attrs.a_color;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        float2 main(const Varyings varyings, out half4 color) {
            color = half4(varyings.color);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> fillExtrusionMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, fillExtrusionZ), SkString("a_z")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, color), SkString("a_color")}};
    const Varying varyings[] = {{Varying::Type::kFloat4, SkString("color")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, attrs.a_z, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.color = attrs.a_color;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        float2 main(const Varyings varyings, out half4 color) {
            color = half4(varyings.color);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> fillExtrusionPatternMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, fillExtrusionZ), SkString("a_z")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, fillPatternPosA), SkString("a_pattern_pos_a")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, fillPatternPosB), SkString("a_pattern_pos_b")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, fillPatternFrom), SkString("a_pattern_from")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, fillPatternTo), SkString("a_pattern_to")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("pos_a")},
                                {Varying::Type::kFloat2, SkString("pos_b")},
                                {Varying::Type::kFloat4, SkString("pattern_from")},
                                {Varying::Type::kFloat4, SkString("pattern_to")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, attrs.a_z, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.pos_a = attrs.a_pattern_pos_a;
            varyings.pos_b = attrs.a_pattern_pos_b;
            varyings.pattern_from = attrs.a_pattern_from;
            varyings.pattern_to = attrs.a_pattern_to;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;
        uniform float u_fade;
        uniform float u_opacity;

        float2 mod_positive(float2 x, float y) {
            return x - y * floor(x / y);
        }

        float2 mix2(float2 a, float2 b, float2 t) {
            return a + (b - a) * t;
        }

        float2 main(const Varyings varyings, out half4 color) {
            float2 imagecoord_a = mod_positive(varyings.pos_a, 1.0);
            float2 imagecoord_b = mod_positive(varyings.pos_b, 1.0);
            float2 pos_a = mix2(varyings.pattern_from.xy, varyings.pattern_from.zw, imagecoord_a);
            float2 pos_b = mix2(varyings.pattern_to.xy, varyings.pattern_to.zw, imagecoord_b);
            color = half4(mix(u_image.eval(pos_a), u_image.eval(pos_b), u_fade) * u_opacity);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> lineMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, color), SkString("a_color")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, lineNormal), SkString("a_normal")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, lineWidth), SkString("a_width")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, lineBlur), SkString("a_blur")}};
    const Varying varyings[] = {{Varying::Type::kFloat4, SkString("color")},
                                {Varying::Type::kFloat2, SkString("normal")},
                                {Varying::Type::kFloat2, SkString("width")},
                                {Varying::Type::kFloat, SkString("blur")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.color = attrs.a_color;
            varyings.normal = attrs.a_normal;
            varyings.width = attrs.a_width;
            varyings.blur = attrs.a_blur;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        float2 main(const Varyings varyings, out half4 color) {
            float dist = length(varyings.normal) * varyings.width.x;
            float blur = varyings.blur + 1.0;
            float alpha = clamp(min(dist - (varyings.width.y - blur), varyings.width.x - dist) / blur, 0.0, 1.0);
            color = half4(varyings.color * alpha);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> lineGradientMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, color), SkString("a_color")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, lineNormal), SkString("a_normal")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, lineWidth), SkString("a_width")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, lineBlur), SkString("a_blur")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, lineProgress), SkString("a_progress")}};
    const Varying varyings[] = {{Varying::Type::kFloat4, SkString("color")},
                                {Varying::Type::kFloat2, SkString("normal")},
                                {Varying::Type::kFloat2, SkString("width")},
                                {Varying::Type::kFloat, SkString("blur")},
                                {Varying::Type::kFloat, SkString("progress")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.color = attrs.a_color;
            varyings.normal = attrs.a_normal;
            varyings.width = attrs.a_width;
            varyings.blur = attrs.a_blur;
            varyings.progress = attrs.a_progress;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_gradient;

        float2 main(const Varyings varyings, out half4 color) {
            float dist = length(varyings.normal) * varyings.width.x;
            float blur = varyings.blur + 1.0;
            float alpha = clamp(min(dist - (varyings.width.y - blur), varyings.width.x - dist) / blur, 0.0, 1.0);
            float x = clamp(varyings.progress, 0.0, 1.0) * 255.0;
            color = half4(u_gradient.eval(float2(x, 0.5)) * alpha * varyings.color.a);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> linePatternMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, color), SkString("a_color")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, lineNormal), SkString("a_normal")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, lineWidth), SkString("a_width")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, lineBlur), SkString("a_blur")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, lineProgress), SkString("a_progress")}};
    const Varying varyings[] = {{Varying::Type::kFloat4, SkString("color")},
                                {Varying::Type::kFloat2, SkString("normal")},
                                {Varying::Type::kFloat2, SkString("width")},
                                {Varying::Type::kFloat, SkString("blur")},
                                {Varying::Type::kFloat, SkString("progress")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.color = attrs.a_color;
            varyings.normal = attrs.a_normal;
            varyings.width = attrs.a_width;
            varyings.blur = attrs.a_blur;
            varyings.progress = attrs.a_progress;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;
        uniform float4 u_pattern_from;
        uniform float4 u_pattern_to;
        uniform float4 u_pattern_scale;
        uniform float2 u_texsize;
        uniform float u_fade;

        float mod_positive(float x, float y) {
            return x - y * floor(x / y);
        }

        float2 mix2(float2 a, float2 b, float2 t) {
            return a + (b - a) * t;
        }

        float2 pattern_pos(float4 pattern, float scale, const Varyings varyings) {
            float2 pattern_tl = pattern.xy;
            float2 pattern_br = pattern.zw;
            float pixel_ratio = u_pattern_scale.x;
            float tile_zoom_ratio = u_pattern_scale.y;
            float2 display_size = (pattern_br - pattern_tl) / pixel_ratio;
            float2 pattern_size = float2(display_size.x * scale / tile_zoom_ratio, display_size.y);
            float linesofar = varyings.progress * 32767.0;
            float x = mod_positive(linesofar / pattern_size.x, 1.0);
            float y = 0.5 + (varyings.normal.y * clamp(varyings.width.x, 0.0, (pattern_size.y + 2.0) / 2.0) / pattern_size.y);
            return mix2(pattern_tl, pattern_br, float2(x, y));
        }

        float2 main(const Varyings varyings, out half4 color) {
            float dist = length(varyings.normal) * varyings.width.x;
            float blur = varyings.blur + 1.0;
            float alpha = clamp(min(dist - (varyings.width.y - blur), varyings.width.x - dist) / blur, 0.0, 1.0);
            float2 pos_a = pattern_pos(u_pattern_from, u_pattern_scale.z, varyings);
            float2 pos_b = pattern_pos(u_pattern_to, u_pattern_scale.w, varyings);
            float4 sampled = mix(u_image.eval(pos_a), u_image.eval(pos_b), u_fade);
            color = half4(sampled * alpha * varyings.color.a);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> lineSDFMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, color), SkString("a_color")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, lineNormal), SkString("a_normal")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, lineWidth), SkString("a_width")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, lineBlur), SkString("a_blur")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, lineProgress), SkString("a_progress")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, lineFloorWidth), SkString("a_floorwidth")}};
    const Varying varyings[] = {{Varying::Type::kFloat4, SkString("color")},
                                {Varying::Type::kFloat2, SkString("normal")},
                                {Varying::Type::kFloat2, SkString("width")},
                                {Varying::Type::kFloat3, SkString("line_metrics")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.color = attrs.a_color;
            varyings.normal = attrs.a_normal;
            varyings.width = attrs.a_width;
            varyings.line_metrics = float3(attrs.a_blur, attrs.a_progress, attrs.a_floorwidth);
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;
        uniform float2 u_patternscale_a;
        uniform float2 u_patternscale_b;
        uniform float u_tex_y_a;
        uniform float u_tex_y_b;
        uniform float u_sdfgamma;
        uniform float u_mix;
        uniform float2 u_sdf_texsize;

        float smooth_edge(float edge0, float edge1, float x) {
            float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        }

        float2 sdf_pos(float2 patternscale, float tex_y, const Varyings varyings) {
            float linesofar = varyings.line_metrics.y * 32767.0;
            return float2(linesofar * patternscale.x / varyings.line_metrics.z,
                          varyings.normal.y * patternscale.y + tex_y);
        }

        float2 main(const Varyings varyings, out half4 color) {
            float dist = length(varyings.normal) * varyings.width.x;
            float blur = varyings.line_metrics.x + 1.0;
            float alpha = clamp(min(dist - (varyings.width.y - blur), varyings.width.x - dist) / blur, 0.0, 1.0);
            float4 sample_a = u_image.eval(sdf_pos(u_patternscale_a, u_tex_y_a, varyings) * u_sdf_texsize);
            float4 sample_b = u_image.eval(sdf_pos(u_patternscale_b, u_tex_y_b, varyings) * u_sdf_texsize);
            float sdfdist_a = max(max(sample_a.r, sample_a.g), max(sample_a.b, sample_a.a));
            float sdfdist_b = max(max(sample_b.r, sample_b.g), max(sample_b.b, sample_b.a));
            float sdfdist = mix(sdfdist_a, sdfdist_b, u_mix);
            alpha *= smooth_edge(0.5 - u_sdfgamma / varyings.line_metrics.z,
                                 0.5 + u_sdfgamma / varyings.line_metrics.z,
                                 sdfdist);
            color = half4(varyings.color * alpha);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> circleMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, circleExtrude), SkString("a_extrude")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, circleColor), SkString("a_color")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, circleStrokeColor), SkString("a_stroke_color")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, circleData), SkString("a_data")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("extrude")},
                                {Varying::Type::kFloat4, SkString("color")},
                                {Varying::Type::kFloat4, SkString("stroke_color")},
                                {Varying::Type::kFloat4, SkString("data")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float2 u_extrude_scale;
        uniform float u_camera_to_center_distance;
        uniform float u_scale_with_map;
        uniform float u_pitch_with_map;

        float2 clip_to_screen(float4 position) {
            float inv_w = position.w == 0.0 ? 1.0 : 1.0 / position.w;
            float2 ndc = position.xy * inv_w;
            return float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                          (0.5 - ndc.y * 0.5) * u_viewport.y);
        }

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float radius = attrs.a_data.x;
            float stroke_width = attrs.a_data.w;
            float2 scaled_extrude = attrs.a_extrude * u_extrude_scale;
            float4 projected;
            if (u_pitch_with_map > 0.5) {
                float2 corner_position = attrs.a_pos;
                if (u_scale_with_map > 0.5) {
                    corner_position += scaled_extrude * (radius + stroke_width);
                } else {
                    float4 projected_center = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
                    corner_position += scaled_extrude * (radius + stroke_width) *
                                       (projected_center.w / u_camera_to_center_distance);
                }
                projected = u_matrix * float4(corner_position, 0.0, 1.0);
            } else {
                projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
                float factor = u_scale_with_map > 0.5 ? u_camera_to_center_distance : projected.w;
                projected.xy += scaled_extrude * (radius + stroke_width) * factor;
            }
            varyings.position = clip_to_screen(projected);
            varyings.extrude = attrs.a_extrude;
            varyings.color = attrs.a_color;
            varyings.stroke_color = attrs.a_stroke_color;
            varyings.data = attrs.a_data;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        float smooth_edge(float edge0, float edge1, float x) {
            float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        }

        float2 main(const Varyings varyings, out half4 color) {
            float radius = varyings.data.x;
            float blur = varyings.data.y;
            float opacity = varyings.data.z;
            float stroke_width = varyings.data.w;
            float extrude_length = length(varyings.extrude);
            float antialiasblur = 1.0 / max(radius + stroke_width, 1.0);
            float antialiased_blur = -max(blur, antialiasblur);
            float opacity_t = smooth_edge(0.0, antialiased_blur, extrude_length - 1.0);
            float color_t = stroke_width < 0.01 ? 0.0 : smooth_edge(antialiased_blur, 0.0, extrude_length - radius / (radius + stroke_width));
            float fill_alpha = varyings.color.a * opacity;
            float stroke_alpha = varyings.stroke_color.a;
            float4 fill = float4(varyings.color.rgb * fill_alpha, fill_alpha);
            float4 stroke = float4(varyings.stroke_color.rgb * stroke_alpha, stroke_alpha);
            color = half4(mix(fill, stroke, color_t) * opacity_t);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> rasterMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, rasterTexturePos), SkString("a_texture_pos")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("pos0")},
                                {Varying::Type::kFloat2, SkString("pos1")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float2 u_tl_parent;
        uniform float u_scale_parent;
        uniform float u_buffer_scale;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.pos0 = (((attrs.a_texture_pos / 8192.0) - 0.5) / u_buffer_scale) + 0.5;
            varyings.pos1 = (varyings.pos0 * u_scale_parent) + u_tl_parent;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image0;
        uniform shader u_image1;
        uniform float2 u_texsize0;
        uniform float2 u_texsize1;
        uniform float4 u_spin_weights;
        uniform float u_fade_t;
        uniform float u_opacity;
        uniform float u_brightness_low;
        uniform float u_brightness_high;
        uniform float u_saturation_factor;
        uniform float u_contrast_factor;

        float3 spin_rgb(float3 rgb) {
            return float3(dot(rgb, u_spin_weights.xyz),
                          dot(rgb, u_spin_weights.zxy),
                          dot(rgb, u_spin_weights.yzx));
        }

        float4 unpremultiply(float4 color) {
            if (color.a > 0.0) {
                color.rgb = color.rgb / color.a;
            }
            return color;
        }

        float2 main(const Varyings varyings, out half4 color) {
            float4 color0 = unpremultiply(u_image0.eval(varyings.pos0 * u_texsize0));
            float4 color1 = unpremultiply(u_image1.eval(varyings.pos1 * u_texsize1));
            float4 mixed = mix(color0, color1, u_fade_t);
            mixed.a *= u_opacity;

            float3 rgb = spin_rgb(mixed.rgb);
            float average = (mixed.r + mixed.g + mixed.b) / 3.0;
            rgb += (average - rgb) * u_saturation_factor;
            rgb = (rgb - 0.5) * u_contrast_factor + 0.5;
            rgb = mix(float3(u_brightness_low), float3(u_brightness_high), rgb);

            color = half4(float4(rgb * mixed.a, mixed.a));
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> colorReliefMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, rasterTexturePos), SkString("a_texture_pos")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("dem_pos")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float2 u_dimension;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);

            float2 epsilon = 1.0 / u_dimension;
            float scale = (u_dimension.x - 2.0) / u_dimension.x;
            varyings.dem_pos = (attrs.a_pos / 8192.0) * scale + epsilon;
            if (attrs.a_pos.y < -32767.5) varyings.dem_pos.y = 0.0;
            if (attrs.a_pos.y > 32766.5) varyings.dem_pos.y = 1.0;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;
        uniform shader u_elevation_stops;
        uniform shader u_color_stops;
        uniform float4 u_unpack;
        uniform float2 u_dimension;
        uniform int u_color_ramp_size;
        uniform float u_opacity;
        uniform float u_elevation_min;
        uniform float u_elevation_scale;

        float get_elevation(float2 coord) {
            float4 data = u_image.eval(coord * u_dimension) * 255.0;
            data.a = -1.0;
            return dot(data, u_unpack);
        }

        float get_elevation_stop(int stop) {
            return u_elevation_stops.eval(float2(float(stop) + 0.5, 0.5)).r;
        }

        float4 get_color_stop(int stop) {
            return u_color_stops.eval(float2(float(stop) + 0.5, 0.5));
        }

        float2 main(const Varyings varyings, out half4 color) {
            float elevation = (get_elevation(varyings.dem_pos) - u_elevation_min) * u_elevation_scale;
            int left = 0;
            int right = u_color_ramp_size - 1;
            if (right < 0) {
                right = 0;
            }
            for (int i = 0; i < 16; ++i) {
                if (right - left > 1) {
                    int middle = (right + left) / 2;
                    if (elevation < get_elevation_stop(middle)) {
                        right = middle;
                    } else {
                        left = middle;
                    }
                }
            }

            float elevation_left = get_elevation_stop(left);
            float elevation_right = get_elevation_stop(right);
            float denom = elevation_right - elevation_left;
            float t = abs(denom) < 0.0001 ? 0.0 : clamp((elevation - elevation_left) / denom, 0.0, 1.0);
            color = half4(mix(get_color_stop(left), get_color_stop(right), t) * u_opacity);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> hillshadePrepareMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, rasterTexturePos), SkString("a_texture_pos")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("dem_pos")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float2 u_dimension;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);

            float2 epsilon = 1.0 / u_dimension;
            float scale = (u_dimension.x - 2.0) / u_dimension.x;
            varyings.dem_pos = (attrs.a_texture_pos / 8192.0) * scale + epsilon;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;
        uniform float4 u_unpack;
        uniform float2 u_dimension;
        uniform float u_zoom;

        float get_elevation(float2 coord) {
            float4 data = u_image.eval(coord * u_dimension) * 255.0;
            data.a = -1.0;
            return dot(data, u_unpack);
        }

        float2 main(const Varyings varyings, out half4 color) {
            float2 epsilon = 1.0 / u_dimension;
            float tile_size = u_dimension.x - 2.0;

            float a = get_elevation(varyings.dem_pos + float2(-epsilon.x, -epsilon.y));
            float b = get_elevation(varyings.dem_pos + float2(0.0, -epsilon.y));
            float c = get_elevation(varyings.dem_pos + float2(epsilon.x, -epsilon.y));
            float d = get_elevation(varyings.dem_pos + float2(-epsilon.x, 0.0));
            float f = get_elevation(varyings.dem_pos + float2(epsilon.x, 0.0));
            float g = get_elevation(varyings.dem_pos + float2(-epsilon.x, epsilon.y));
            float h = get_elevation(varyings.dem_pos + float2(0.0, epsilon.y));
            float i = get_elevation(varyings.dem_pos + float2(epsilon.x, epsilon.y));

            float exaggeration_factor = u_zoom < 2.0 ? 0.4 : (u_zoom < 4.5 ? 0.35 : 0.3);
            float exaggeration = u_zoom < 15.0 ? (u_zoom - 15.0) * exaggeration_factor : 0.0;
            float2 deriv = float2((c + f + f + i) - (a + d + d + g),
                                  (g + h + h + i) - (a + b + b + c)) *
                           tile_size / pow(2.0, exaggeration + (28.2562 - u_zoom));

            color = half4(clamp(float4(deriv.x / 8.0 + 0.5,
                                       deriv.y / 8.0 + 0.5,
                                       1.0,
                                       1.0), 0.0, 1.0));
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> hillshadeMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, rasterTexturePos), SkString("a_texture_pos")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("dem_pos")},
                                {Varying::Type::kFloat2, SkString("tile_pos")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float2 u_dimension;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            float2 epsilon = 1.0 / u_dimension;
            float scale = (u_dimension.x - 2.0) / u_dimension.x;
            varyings.dem_pos = (attrs.a_texture_pos / 8192.0) * scale + epsilon;
            varyings.tile_pos = attrs.a_texture_pos / 8192.0;
            varyings.tile_pos.y = 1.0 - varyings.tile_pos.y;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;
        uniform float2 u_dimension;
        uniform float4 u_unpack;
        uniform float u_zoom;
        uniform float2 u_latrange;
        uniform float u_exaggeration;
        uniform int u_method;
        uniform int u_num_lights;
        uniform float4 u_accent;
        uniform float4 u_altitudes;
        uniform float4 u_azimuths;
        uniform float4 u_shadow0;
        uniform float4 u_shadow1;
        uniform float4 u_shadow2;
        uniform float4 u_shadow3;
        uniform float4 u_highlight0;
        uniform float4 u_highlight1;
        uniform float4 u_highlight2;
        uniform float4 u_highlight3;

        const float PI = 3.141592653589793;
        const int STANDARD = 0;
        const int COMBINED = 1;
        const int IGOR = 2;
        const int MULTIDIRECTIONAL = 3;
        const int BASIC = 4;

        float mod_positive(float x, float y) {
            return x - y * floor(x / y);
        }

        float get_component(float4 value, int index) {
            return index == 0 ? value.x : (index == 1 ? value.y : (index == 2 ? value.z : value.w));
        }

        float4 get_shadow(int index) {
            return index == 0 ? u_shadow0 : (index == 1 ? u_shadow1 : (index == 2 ? u_shadow2 : u_shadow3));
        }

        float4 get_highlight(int index) {
            return index == 0 ? u_highlight0 : (index == 1 ? u_highlight1 : (index == 2 ? u_highlight2 : u_highlight3));
        }

        float get_aspect(float2 deriv) {
            return deriv.x != 0.0 ? atan(deriv.y, -deriv.x) : PI / 2.0 * (deriv.y > 0.0 ? 1.0 : -1.0);
        }

        float get_elevation(float2 coord) {
            float4 data = u_image.eval(coord * u_dimension) * 255.0;
            data.a = -1.0;
            return dot(data, u_unpack);
        }

        float2 get_deriv(float2 dem_pos) {
            float2 epsilon = 1.0 / u_dimension;
            float tile_size = u_dimension.x - 2.0;
            float a = get_elevation(dem_pos + float2(-epsilon.x, -epsilon.y));
            float b = get_elevation(dem_pos + float2(0.0, -epsilon.y));
            float c = get_elevation(dem_pos + float2(epsilon.x, -epsilon.y));
            float d = get_elevation(dem_pos + float2(-epsilon.x, 0.0));
            float f = get_elevation(dem_pos + float2(epsilon.x, 0.0));
            float g = get_elevation(dem_pos + float2(-epsilon.x, epsilon.y));
            float h = get_elevation(dem_pos + float2(0.0, epsilon.y));
            float i = get_elevation(dem_pos + float2(epsilon.x, epsilon.y));
            float exaggeration = u_zoom < 15.0 ? (u_zoom - 15.0) * (u_zoom < 2.0 ? 0.4 : (u_zoom < 4.5 ? 0.35 : 0.3)) : 0.0;
            return float2((c + f + f + i) - (a + d + d + g),
                          (g + h + h + i) - (a + b + b + c)) *
                   tile_size / pow(2.0, exaggeration + (28.2562 - u_zoom));
        }

        float4 standard_hillshade(float2 deriv) {
            float azimuth = u_azimuths.x + PI;
            float slope = atan(0.625 * length(deriv));
            float aspect = get_aspect(deriv);
            float intensity = u_exaggeration;
            float base = 1.875 - intensity * 1.75;
            float max_value = 0.5 * PI;
            float scaled_slope = abs(intensity - 0.5) > 1e-6 ?
                                 ((pow(base, slope) - 1.0) / (pow(base, max_value) - 1.0)) * max_value :
                                 slope;
            float accent = cos(scaled_slope);
            float4 accent_color = (1.0 - accent) * u_accent * clamp(intensity * 2.0, 0.0, 1.0);
            float shade = abs(mod_positive((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
            float4 shade_color = mix(u_shadow0, u_highlight0, shade) * sin(scaled_slope) * clamp(intensity * 2.0, 0.0, 1.0);
            return accent_color * (1.0 - shade_color.a) + shade_color;
        }

        float4 basic_hillshade(float2 deriv) {
            deriv = deriv * u_exaggeration * 2.0;
            float azimuth = u_azimuths.x + PI;
            float cos_az = cos(azimuth);
            float sin_az = sin(azimuth);
            float cos_alt = cos(u_altitudes.x);
            float sin_alt = sin(u_altitudes.x);
            float cang = (sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv));
            float shade = clamp(cang, 0.0, 1.0);
            return shade > 0.5 ? u_highlight0 * (2.0 * shade - 1.0) : u_shadow0 * (1.0 - 2.0 * shade);
        }

        float4 combined_hillshade(float2 deriv) {
            deriv = deriv * u_exaggeration * 2.0;
            float azimuth = u_azimuths.x + PI;
            float cos_az = cos(azimuth);
            float sin_az = sin(azimuth);
            float cos_alt = cos(u_altitudes.x);
            float sin_alt = sin(u_altitudes.x);
            float cang = acos((sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv)));
            cang = clamp(cang, 0.0, PI / 2.0);
            float shade = cang * atan(length(deriv)) * 4.0 / PI / PI;
            float highlight = (PI / 2.0 - cang) * atan(length(deriv)) * 4.0 / PI / PI;
            return u_shadow0 * shade + u_highlight0 * highlight;
        }

        float4 igor_hillshade(float2 deriv) {
            deriv = deriv * u_exaggeration * 2.0;
            float aspect = get_aspect(deriv);
            float azimuth = u_azimuths.x + PI;
            float slope_strength = atan(length(deriv)) * 2.0 / PI;
            float aspect_strength = 1.0 - abs(mod_positive((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
            float shadow_strength = slope_strength * aspect_strength;
            float highlight_strength = slope_strength * (1.0 - aspect_strength);
            return u_shadow0 * shadow_strength + u_highlight0 * highlight_strength;
        }

        float4 multidirectional_hillshade(float2 deriv) {
            deriv = deriv * u_exaggeration * 2.0;
            float4 total_color = float4(0.0);
            int num_lights = u_num_lights < 4 ? u_num_lights : 4;
            for (int i = 0; i < 4; ++i) {
                if (i < num_lights) {
                    float altitude = get_component(u_altitudes, i);
                    float azimuth = get_component(u_azimuths, i);
                    float cos_alt = cos(altitude);
                    float sin_alt = sin(altitude);
                    float cos_az = -cos(azimuth);
                    float sin_az = -sin(azimuth);
                    float cang = (sin_alt - (deriv.y * cos_az * cos_alt - deriv.x * sin_az * cos_alt)) / sqrt(1.0 + dot(deriv, deriv));
                    float shade = clamp(cang, 0.0, 1.0);
                    total_color += (shade > 0.5 ? get_highlight(i) * (2.0 * shade - 1.0) : get_shadow(i) * (1.0 - 2.0 * shade)) / float(num_lights);
                }
            }
            return total_color;
        }

        float2 main(const Varyings varyings, out half4 color) {
            float latitude = (u_latrange.x - u_latrange.y) * varyings.tile_pos.y + u_latrange.y;
            float scale_factor = cos(latitude * PI / 180.0);
            float2 deriv = get_deriv(varyings.dem_pos) / scale_factor;
            float4 result = u_method == BASIC ? basic_hillshade(deriv) :
                            (u_method == COMBINED ? combined_hillshade(deriv) :
                             (u_method == IGOR ? igor_hillshade(deriv) :
                              (u_method == MULTIDIRECTIONAL ? multidirectional_hillshade(deriv) : standard_hillshade(deriv))));
            color = half4(result);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> heatmapMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, heatmapWeight), SkString("a_weight")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, heatmapRadius), SkString("a_radius")}};
    const Varying varyings[] = {{Varying::Type::kFloat, SkString("weight")},
                                {Varying::Type::kFloat2, SkString("extrude")}};

    const SkString vertexShader(R"(
        const float ZERO = 1.0 / 255.0 / 16.0;
        const float GAUSS_COEF = 0.3989422804014327;

        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float u_extrude_scale;
        uniform float u_weight;
        uniform float u_radius;
        uniform float u_intensity;
        uniform float u_weight_t;
        uniform float u_radius_t;

        float unpack_mix_float(float2 value, float t) {
            return mix(value.x, value.y, t);
        }

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float weight = unpack_mix_float(attrs.a_weight, u_weight_t);
            float radius = unpack_mix_float(attrs.a_radius, u_radius_t);
            float2 unscaled_extrude = mod(attrs.a_pos, 2.0) * 2.0 - 1.0;
            float S = sqrt(-2.0 * log(ZERO / (max(weight, ZERO) * max(u_intensity, ZERO) * GAUSS_COEF))) / 3.0;
            varyings.extrude = S * unscaled_extrude;
            varyings.weight = weight;
            float2 scaled_extrude = varyings.extrude * radius * u_extrude_scale;
            float2 tile_pos = floor(attrs.a_pos * 0.5) + scaled_extrude;

            float4 projected = u_matrix * float4(tile_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        const float GAUSS_COEF = 0.3989422804014327;
        uniform float u_intensity;

        float2 main(const Varyings varyings, out half4 color) {
            float d = -0.5 * 3.0 * 3.0 * dot(varyings.extrude, varyings.extrude);
            float val = varyings.weight * u_intensity * GAUSS_COEF * exp(d);
            color = half4(val, 1.0, 1.0, 1.0);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> heatmapTextureMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("pos")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float2 u_world_size;

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float2 pos = attrs.a_pos;
            float4 projected = u_matrix * float4(pos * u_world_size, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.pos = pos;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;
        uniform shader u_color_ramp;
        uniform float2 u_image_size;
        uniform float u_opacity;

        float2 main(const Varyings varyings, out half4 color) {
            float t = u_image.eval(varyings.pos * u_image_size).r;
            color = half4(u_color_ramp.eval(float2(t * 256.0, 0.5)) * u_opacity);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> symbolIconMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat4, offsetof(MeshVertex, symbolPosOffset), SkString("a_pos_offset")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolData), SkString("a_data")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolPixelOffset), SkString("a_pixel_offset")},
                                    {Attribute::Type::kFloat3, offsetof(MeshVertex, symbolProjectedPos), SkString("a_projected_pos")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, symbolFadeOpacity), SkString("a_fade_opacity")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, symbolOpacity), SkString("a_opacity")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("tex")},
                                {Varying::Type::kFloat, SkString("opacity")}};

    const SkString vertexShader(R"(
        const float c_offscreen_degenerate_triangle_location = -2.0;

        uniform float4x4 u_matrix;
        uniform float4x4 u_label_plane_matrix;
        uniform float4x4 u_coord_matrix;
        uniform float2 u_viewport;
        uniform float u_camera_to_center_distance;
        uniform float u_symbol_fade_change;
        uniform float u_aspect_ratio;
        uniform float u_rotate_symbol;
        uniform float u_pitch_with_map;
        uniform float u_is_size_zoom_constant;
        uniform float u_is_size_feature_constant;
        uniform float u_is_offset;
        uniform float u_size_t;
        uniform float u_size;
        uniform float u_opacity_t;

        float2 unpack_opacity(float packed_opacity) {
            float value = floor(packed_opacity);
            return float2(floor(value / 2.0) / 127.0, mod(value, 2.0));
        }

        float unpack_mix_float(float2 value, float t) {
            return mix(value.x, value.y, t);
        }

        float2 project_to_screen(float4 position) {
            float inv_w = position.w == 0.0 ? 1.0 : 1.0 / position.w;
            float2 ndc = position.xy * inv_w;
            return float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                          (0.5 - ndc.y * 0.5) * u_viewport.y);
        }

        Varyings main(const Attributes attrs) {
            Varyings varyings;

            float2 raw_fade_opacity = unpack_opacity(attrs.a_fade_opacity);
            float fade_change = raw_fade_opacity.y > 0.5 ? u_symbol_fade_change : -u_symbol_fade_change;
            float fade_opacity = clamp(raw_fade_opacity.x + fade_change, 0.0, 1.0);
            float opacity = unpack_mix_float(attrs.a_opacity, u_opacity_t) * fade_opacity;
            if (opacity == 0.0) {
                varyings.position = float2(c_offscreen_degenerate_triangle_location,
                                           c_offscreen_degenerate_triangle_location);
                varyings.tex = attrs.a_data.xy;
                varyings.opacity = opacity;
                return varyings;
            }

            float2 a_pos = attrs.a_pos_offset.xy;
            float2 a_offset = attrs.a_pos_offset.zw;
            float2 a_tex = attrs.a_data.xy;
            float2 a_size = attrs.a_data.zw;
            float a_size_min = floor(a_size.x * 0.5);
            float2 a_pxoffset = attrs.a_pixel_offset.xy;
            float2 a_min_font_scale = attrs.a_pixel_offset.zw / 256.0;
            float segment_angle = -attrs.a_projected_pos.z;

            float size;
            if (u_is_size_zoom_constant < 0.5 && u_is_size_feature_constant < 0.5) {
                size = mix(a_size_min, a_size.y, u_size_t) / 128.0;
            } else if (u_is_size_zoom_constant >= 0.5 && u_is_size_feature_constant < 0.5) {
                size = a_size_min / 128.0;
            } else {
                size = u_size;
            }

            float4 projected_point = u_matrix * float4(a_pos, 0.0, 1.0);
            float camera_to_anchor_distance = projected_point.w;
            float distance_ratio = u_pitch_with_map >= 0.5
                ? camera_to_anchor_distance / u_camera_to_center_distance
                : u_camera_to_center_distance / camera_to_anchor_distance;
            float perspective_ratio = clamp(0.5 + 0.5 * distance_ratio, 0.0, 4.0);
            if (u_is_offset < 0.5) {
                size *= perspective_ratio;
            }

            float symbol_rotation = 0.0;
            if (u_rotate_symbol >= 0.5) {
                float4 offset_projected_point = u_matrix * float4(a_pos + float2(1.0, 0.0), 0.0, 1.0);
                float2 a = projected_point.xy / projected_point.w;
                float2 b = offset_projected_point.xy / offset_projected_point.w;
                symbol_rotation = atan((b.y - a.y) / u_aspect_ratio, b.x - a.x);
            }

            float angle = segment_angle + symbol_rotation;
            float angle_sin = sin(angle);
            float angle_cos = cos(angle);

            float4 projected_pos = u_label_plane_matrix * float4(attrs.a_projected_pos.xy, 0.0, 1.0);
            float2 pos0 = projected_pos.xy / projected_pos.w;
            float2 pos_offset = a_offset * max(a_min_font_scale, float2(size)) / 32.0 + a_pxoffset / 16.0;
            float2 rotated_offset = float2(angle_cos * pos_offset.x - angle_sin * pos_offset.y,
                                           angle_sin * pos_offset.x + angle_cos * pos_offset.y);
            float4 position = u_coord_matrix * float4(pos0 + rotated_offset, 0.0, 1.0);

            varyings.position = project_to_screen(position);
            varyings.tex = a_tex;
            varyings.opacity = opacity;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;

        float2 main(const Varyings varyings, out half4 color) {
            color = half4(u_image.eval(varyings.tex) * varyings.opacity);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> symbolSDFMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat4, offsetof(MeshVertex, symbolPosOffset), SkString("a_pos_offset")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolData), SkString("a_data")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolPixelOffset), SkString("a_pixel_offset")},
                                    {Attribute::Type::kFloat3, offsetof(MeshVertex, symbolProjectedPos), SkString("a_projected_pos")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, symbolFadeOpacity), SkString("a_fade_opacity")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolFillColor), SkString("a_fill_color")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolHaloColor), SkString("a_halo_color")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolHaloWidth), SkString("a_paint")}};
    const Varying varyings[] = {{Varying::Type::kFloat4, SkString("geometry")},
                                {Varying::Type::kFloat4, SkString("fill_color")},
                                {Varying::Type::kFloat4, SkString("halo_color")},
                                {Varying::Type::kFloat4, SkString("paint")}};

    const SkString vertexShader(R"(
        const float c_offscreen_degenerate_triangle_location = -2.0;

        uniform float4x4 u_matrix;
        uniform float4x4 u_label_plane_matrix;
        uniform float4x4 u_coord_matrix;
        uniform float2 u_viewport;
        uniform float u_camera_to_center_distance;
        uniform float u_symbol_fade_change;
        uniform float u_aspect_ratio;
        uniform float u_rotate_symbol;
        uniform float u_pitch_with_map;
        uniform float u_is_text;
        uniform float u_is_size_zoom_constant;
        uniform float u_is_size_feature_constant;
        uniform float u_is_offset;
        uniform float u_size_t;
        uniform float u_size;

        float2 unpack_opacity(float packed_opacity) {
            float value = floor(packed_opacity);
            return float2(floor(value / 2.0) / 127.0, mod(value, 2.0));
        }

        float2 project_to_screen(float4 position) {
            float inv_w = position.w == 0.0 ? 1.0 : 1.0 / position.w;
            float2 ndc = position.xy * inv_w;
            return float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                          (0.5 - ndc.y * 0.5) * u_viewport.y);
        }

        Varyings main(const Attributes attrs) {
            Varyings varyings;

            float2 raw_fade_opacity = unpack_opacity(attrs.a_fade_opacity);
            float fade_change = raw_fade_opacity.y > 0.5 ? u_symbol_fade_change : -u_symbol_fade_change;
            float fade_opacity = clamp(raw_fade_opacity.x + fade_change, 0.0, 1.0);
            if (fade_opacity == 0.0) {
                varyings.position = float2(c_offscreen_degenerate_triangle_location,
                                           c_offscreen_degenerate_triangle_location);
                return varyings;
            }

            float2 a_pos = attrs.a_pos_offset.xy;
            float2 a_offset = attrs.a_pos_offset.zw;
            float2 a_tex = attrs.a_data.xy;
            float2 a_size = attrs.a_data.zw;
            float a_size_min = floor(a_size.x * 0.5);
            float2 a_pxoffset = attrs.a_pixel_offset.xy;
            float segment_angle = -attrs.a_projected_pos.z;

            float size;
            if (u_is_size_zoom_constant < 0.5 && u_is_size_feature_constant < 0.5) {
                size = mix(a_size_min, a_size.y, u_size_t) / 128.0;
            } else if (u_is_size_zoom_constant >= 0.5 && u_is_size_feature_constant < 0.5) {
                size = a_size_min / 128.0;
            } else {
                size = u_size;
            }

            float4 projected_point = u_matrix * float4(a_pos, 0.0, 1.0);
            float camera_to_anchor_distance = projected_point.w;
            float distance_ratio = u_pitch_with_map >= 0.5
                ? camera_to_anchor_distance / u_camera_to_center_distance
                : u_camera_to_center_distance / camera_to_anchor_distance;
            float perspective_ratio = clamp(0.5 + 0.5 * distance_ratio, 0.0, 4.0);
            if (u_is_offset < 0.5) {
                size *= perspective_ratio;
            }

            float font_scale = u_is_text >= 0.5 ? size / 24.0 : size;

            float symbol_rotation = 0.0;
            if (u_rotate_symbol >= 0.5) {
                float4 offset_projected_point = u_matrix * float4(a_pos + float2(1.0, 0.0), 0.0, 1.0);
                float2 a = projected_point.xy / projected_point.w;
                float2 b = offset_projected_point.xy / offset_projected_point.w;
                symbol_rotation = atan((b.y - a.y) / u_aspect_ratio, b.x - a.x);
            }

            float angle = segment_angle + symbol_rotation;
            float angle_sin = sin(angle);
            float angle_cos = cos(angle);

            float4 projected_pos = u_label_plane_matrix * float4(attrs.a_projected_pos.xy, 0.0, 1.0);
            float2 pos_rot = a_offset / 32.0 * font_scale + a_pxoffset;
            float2 rotated_offset = float2(angle_cos * pos_rot.x - angle_sin * pos_rot.y,
                                           angle_sin * pos_rot.x + angle_cos * pos_rot.y);
            float2 pos0 = projected_pos.xy / projected_pos.w + rotated_offset;
            float4 position = u_coord_matrix * float4(pos0, 0.0, 1.0);

            varyings.position = project_to_screen(position);
            varyings.geometry = float4(a_tex, position.w, font_scale);
            varyings.fill_color = attrs.a_fill_color;
            varyings.halo_color = attrs.a_halo_color;
            varyings.paint = float4(attrs.a_paint.xyz, fade_opacity);
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        const float SDF_PX = 8.0;
        uniform shader u_image;
        uniform float u_device_pixel_ratio;
        uniform float u_is_halo;
        uniform float u_gamma_scale;

        float2 main(const Varyings varyings, out half4 color) {
            float edge_gamma = 0.105 / u_device_pixel_ratio;
            float font_scale = varyings.geometry.w;
            float font_gamma = font_scale * u_gamma_scale;
            float fill_gamma = edge_gamma / font_gamma;
            float opacity = varyings.paint.x;
            float halo_width = varyings.paint.y;
            float halo_blur = varyings.paint.z;
            float fade_opacity = varyings.paint.w;
            float halo_gamma = (halo_blur * 1.19 / SDF_PX + edge_gamma) / font_gamma;
            float gamma = u_is_halo >= 0.5 ? halo_gamma : fill_gamma;
            float gamma_scaled = gamma * varyings.geometry.z;
            float4 symbol_color = u_is_halo >= 0.5 ? varyings.halo_color : varyings.fill_color;
            float fill_inner_edge = (256.0 - 64.0) / 256.0;
            float halo_inner_edge = fill_inner_edge + halo_gamma * u_gamma_scale;
            float inner_edge = u_is_halo >= 0.5 ? halo_inner_edge : fill_inner_edge;
            float4 sample = u_image.eval(varyings.geometry.xy);
            float dist = max(max(sample.r, sample.g), max(sample.b, sample.a));
            float alpha = smoothstep(inner_edge - gamma_scaled, inner_edge + gamma_scaled, dist);
            if (u_is_halo >= 0.5) {
                float halo_edge = (6.0 - halo_width / font_scale) / SDF_PX;
                alpha = min(smoothstep(halo_edge - gamma_scaled, halo_edge + gamma_scaled, dist), 1.0 - alpha);
            }
            color = half4(symbol_color * (alpha * opacity * fade_opacity));
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> symbolTextAndIconMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat4, offsetof(MeshVertex, symbolPosOffset), SkString("a_pos_offset")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolData), SkString("a_data")},
                                    {Attribute::Type::kFloat3, offsetof(MeshVertex, symbolProjectedPos), SkString("a_projected_pos")},
                                    {Attribute::Type::kFloat, offsetof(MeshVertex, symbolFadeOpacity), SkString("a_fade_opacity")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolFillColor), SkString("a_fill_color")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolHaloColor), SkString("a_halo_color")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, symbolHaloWidth), SkString("a_paint")}};
    const Varying varyings[] = {{Varying::Type::kFloat4, SkString("geometry")},
                                {Varying::Type::kFloat4, SkString("fill_color")},
                                {Varying::Type::kFloat4, SkString("halo_color")},
                                {Varying::Type::kFloat4, SkString("paint")}};

    const SkString vertexShader(R"(
        const float c_offscreen_degenerate_triangle_location = -2.0;

        uniform float4x4 u_matrix;
        uniform float4x4 u_label_plane_matrix;
        uniform float4x4 u_coord_matrix;
        uniform float2 u_viewport;
        uniform float u_camera_to_center_distance;
        uniform float u_symbol_fade_change;
        uniform float u_aspect_ratio;
        uniform float u_rotate_symbol;
        uniform float u_pitch_with_map;
        uniform float u_is_size_zoom_constant;
        uniform float u_is_size_feature_constant;
        uniform float u_is_offset;
        uniform float u_size_t;
        uniform float u_size;

        float2 unpack_opacity(float packed_opacity) {
            float value = floor(packed_opacity);
            return float2(floor(value / 2.0) / 127.0, mod(value, 2.0));
        }

        float2 project_to_screen(float4 position) {
            float inv_w = position.w == 0.0 ? 1.0 : 1.0 / position.w;
            float2 ndc = position.xy * inv_w;
            return float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                          (0.5 - ndc.y * 0.5) * u_viewport.y);
        }

        Varyings main(const Attributes attrs) {
            Varyings varyings;

            float2 raw_fade_opacity = unpack_opacity(attrs.a_fade_opacity);
            float fade_change = raw_fade_opacity.y > 0.5 ? u_symbol_fade_change : -u_symbol_fade_change;
            float fade_opacity = clamp(raw_fade_opacity.x + fade_change, 0.0, 1.0);
            if (fade_opacity == 0.0) {
                varyings.position = float2(c_offscreen_degenerate_triangle_location,
                                           c_offscreen_degenerate_triangle_location);
                return varyings;
            }

            float2 a_pos = attrs.a_pos_offset.xy;
            float2 a_offset = attrs.a_pos_offset.zw;
            float2 a_tex = attrs.a_data.xy;
            float2 a_size = attrs.a_data.zw;
            float a_size_min = floor(a_size.x * 0.5);
            float is_sdf = a_size.x - 2.0 * a_size_min;
            float segment_angle = -attrs.a_projected_pos.z;

            float size;
            if (u_is_size_zoom_constant < 0.5 && u_is_size_feature_constant < 0.5) {
                size = mix(a_size_min, a_size.y, u_size_t) / 128.0;
            } else if (u_is_size_zoom_constant >= 0.5 && u_is_size_feature_constant < 0.5) {
                size = a_size_min / 128.0;
            } else {
                size = u_size;
            }

            float4 projected_point = u_matrix * float4(a_pos, 0.0, 1.0);
            float camera_to_anchor_distance = projected_point.w;
            float distance_ratio = u_pitch_with_map >= 0.5
                ? camera_to_anchor_distance / u_camera_to_center_distance
                : u_camera_to_center_distance / camera_to_anchor_distance;
            float perspective_ratio = clamp(0.5 + 0.5 * distance_ratio, 0.0, 4.0);
            if (u_is_offset < 0.5) {
                size *= perspective_ratio;
            }

            float font_scale = size / 24.0;

            float symbol_rotation = 0.0;
            if (u_rotate_symbol >= 0.5) {
                float4 offset_projected_point = u_matrix * float4(a_pos + float2(1.0, 0.0), 0.0, 1.0);
                float2 a = projected_point.xy / projected_point.w;
                float2 b = offset_projected_point.xy / offset_projected_point.w;
                symbol_rotation = atan((b.y - a.y) / u_aspect_ratio, b.x - a.x);
            }

            float angle = segment_angle + symbol_rotation;
            float angle_sin = sin(angle);
            float angle_cos = cos(angle);

            float4 projected_pos = u_label_plane_matrix * float4(attrs.a_projected_pos.xy, 0.0, 1.0);
            float2 pos_rot = a_offset / 32.0 * font_scale;
            float2 rotated_offset = float2(angle_cos * pos_rot.x - angle_sin * pos_rot.y,
                                           angle_sin * pos_rot.x + angle_cos * pos_rot.y);
            float2 pos0 = projected_pos.xy / projected_pos.w + rotated_offset;
            float4 position = u_coord_matrix * float4(pos0, 0.0, 1.0);

            varyings.position = project_to_screen(position);
            varyings.geometry = float4(a_tex, position.w, is_sdf < 0.5 ? -font_scale : font_scale);
            varyings.fill_color = attrs.a_fill_color;
            varyings.halo_color = attrs.a_halo_color;
            varyings.paint = float4(attrs.a_paint.xyz, fade_opacity);
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        const float SDF_PX = 8.0;
        uniform shader u_image;
        uniform shader u_icon_image;
        uniform float u_device_pixel_ratio;
        uniform float u_is_halo;
        uniform float u_gamma_scale;

        float2 main(const Varyings varyings, out half4 color) {
            float2 tex = varyings.geometry.xy;
            float font_scale = abs(varyings.geometry.w);
            float opacity = varyings.paint.x;
            float halo_width = varyings.paint.y;
            float halo_blur = varyings.paint.z;
            float fade_opacity = varyings.paint.w;

            if (varyings.geometry.w < 0.0) {
                color = half4(u_icon_image.eval(tex) * (opacity * fade_opacity));
                return varyings.position;
            }

            float edge_gamma = 0.105 / u_device_pixel_ratio;
            float font_gamma = font_scale * u_gamma_scale;
            float fill_gamma = edge_gamma / font_gamma;
            float halo_gamma = (halo_blur * 1.19 / SDF_PX + edge_gamma) / font_gamma;
            float gamma = u_is_halo >= 0.5 ? halo_gamma : fill_gamma;
            float gamma_scaled = gamma * varyings.geometry.z;
            float4 symbol_color = u_is_halo >= 0.5 ? varyings.halo_color : varyings.fill_color;
            float fill_inner_edge = (256.0 - 64.0) / 256.0;
            float halo_inner_edge = fill_inner_edge + halo_gamma * u_gamma_scale;
            float inner_edge = u_is_halo >= 0.5 ? halo_inner_edge : fill_inner_edge;
            float4 sample = u_image.eval(tex);
            float dist = max(max(sample.r, sample.g), max(sample.b, sample.a));
            float alpha = smoothstep(inner_edge - gamma_scaled, inner_edge + gamma_scaled, dist);
            if (u_is_halo >= 0.5) {
                float halo_edge = (6.0 - halo_width / font_scale) / SDF_PX;
                alpha = min(smoothstep(halo_edge - gamma_scaled, halo_edge + gamma_scaled, dist), 1.0 - alpha);
            }
            color = half4(symbol_color * (alpha * opacity * fade_opacity));
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> collisionCircleMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, collisionAnchorPos), SkString("a_anchor_pos")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, collisionExtrude), SkString("a_extrude")},
                                    {Attribute::Type::kFloat2, offsetof(MeshVertex, collisionPlaced), SkString("a_placed")}};
    const Varying varyings[] = {{Varying::Type::kFloat4, SkString("data")},
                                {Varying::Type::kFloat4, SkString("extrude_data")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float2 u_extrude_scale;
        uniform float u_camera_to_center_distance;

        float2 project_to_screen(float4 position) {
            float inv_w = position.w == 0.0 ? 1.0 : 1.0 / position.w;
            float2 ndc = position.xy * inv_w;
            return float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                          (0.5 - ndc.y * 0.5) * u_viewport.y);
        }

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected_point = u_matrix * float4(attrs.a_anchor_pos, 0.0, 1.0);
            float camera_to_anchor_distance = projected_point.w;
            float collision_perspective_ratio = clamp(
                0.5 + 0.5 * (u_camera_to_center_distance / camera_to_anchor_distance), 0.0, 4.0);

            float4 position = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float padding_factor = 1.2;
            position.xy += attrs.a_extrude * u_extrude_scale * padding_factor * position.w * collision_perspective_ratio;

            float radius = abs(attrs.a_extrude.y);
            varyings.position = project_to_screen(position);
            varyings.data = float4(attrs.a_placed, radius, collision_perspective_ratio);
            varyings.extrude_data = float4(attrs.a_extrude * padding_factor,
                                           u_extrude_scale * u_camera_to_center_distance * collision_perspective_ratio);
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform float u_overscale_factor;

        float2 main(const Varyings varyings, out half4 color) {
            float alpha = 0.5;
            float4 ring_color = float4(1.0, 0.0, 0.0, 1.0) * alpha;
            if (varyings.data.x > 0.5) {
                ring_color = float4(0.0, 0.0, 1.0, 0.5) * alpha;
            }
            if (varyings.data.y > 0.5) {
                ring_color *= 0.2;
            }

            float extrude_scale_length = length(varyings.extrude_data.zw);
            float extrude_length = length(varyings.extrude_data.xy) * extrude_scale_length;
            float stroke_width = 15.0 * extrude_scale_length / u_overscale_factor;
            float radius = varyings.data.z * extrude_scale_length;
            float distance_to_edge = abs(extrude_length - radius);
            float opacity_t = smoothstep(-stroke_width, 0.0, -distance_to_edge);
            color = half4(opacity_t * ring_color);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> fillPatternMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, color), SkString("a_color")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, fillPatternFrom), SkString("a_pattern_from")},
                                    {Attribute::Type::kFloat4, offsetof(MeshVertex, fillPatternTo), SkString("a_pattern_to")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("pos_a")},
                                {Varying::Type::kFloat2, SkString("pos_b")},
                                {Varying::Type::kFloat4, SkString("pattern_from")},
                                {Varying::Type::kFloat4, SkString("pattern_to")},
                                {Varying::Type::kFloat, SkString("opacity")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float2 u_pixel_coord_upper;
        uniform float2 u_pixel_coord_lower;
        uniform float u_tile_ratio;
        uniform float u_from_scale;
        uniform float u_to_scale;
        uniform float u_pixel_ratio;

        float2 mod_positive(float2 x, float2 y) {
            return x - y * floor(x / y);
        }

        float2 get_pattern_pos(float2 pattern_size, float2 pos) {
            float2 offset = mod_positive(mod_positive(mod_positive(u_pixel_coord_upper, pattern_size) * 256.0, pattern_size) * 256.0 + u_pixel_coord_lower, pattern_size);
            return (u_tile_ratio * pos + offset) / pattern_size;
        }

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);

            float2 display_size_a = (attrs.a_pattern_from.zw - attrs.a_pattern_from.xy) / u_pixel_ratio;
            float2 display_size_b = (attrs.a_pattern_to.zw - attrs.a_pattern_to.xy) / u_pixel_ratio;
            varyings.pos_a = get_pattern_pos(u_from_scale * display_size_a, attrs.a_pos);
            varyings.pos_b = get_pattern_pos(u_to_scale * display_size_b, attrs.a_pos);
            varyings.pattern_from = attrs.a_pattern_from;
            varyings.pattern_to = attrs.a_pattern_to;
            varyings.opacity = attrs.a_color.a;
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;
        uniform float2 u_texsize;
        uniform float u_fade;

        float2 mod_positive(float2 x, float y) {
            return x - y * floor(x / y);
        }

        float2 mix2(float2 a, float2 b, float2 t) {
            return a + (b - a) * t;
        }

        float2 main(const Varyings varyings, out half4 color) {
            float2 imagecoord_a = mod_positive(varyings.pos_a, 1.0);
            float2 imagecoord_b = mod_positive(varyings.pos_b, 1.0);
            float2 pos_a = mix2(varyings.pattern_from.xy, varyings.pattern_from.zw, imagecoord_a);
            float2 pos_b = mix2(varyings.pattern_to.xy, varyings.pattern_to.zw, imagecoord_b);
            float4 color_a = u_image.eval(pos_a);
            float4 color_b = u_image.eval(pos_b);
            color = half4(mix(color_a, color_b, u_fade) * varyings.opacity);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

sk_sp<SkMeshSpecification> backgroundPatternMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    using Varying = SkMeshSpecification::Varying;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, offsetof(MeshVertex, position), SkString("a_pos")}};
    const Varying varyings[] = {{Varying::Type::kFloat2, SkString("pos_a")},
                                {Varying::Type::kFloat2, SkString("pos_b")}};

    const SkString vertexShader(R"(
        uniform float4x4 u_matrix;
        uniform float2 u_viewport;
        uniform float2 u_pixel_coord_upper;
        uniform float2 u_pixel_coord_lower;
        uniform float u_tile_units_to_pixels;
        uniform float2 u_pattern_size_a;
        uniform float2 u_pattern_size_b;
        uniform float u_scale_a;
        uniform float u_scale_b;

        float2 mod_positive(float2 x, float2 y) {
            return x - y * floor(x / y);
        }

        float2 get_pattern_pos(float2 pattern_size, float2 pos) {
            float2 offset = mod_positive(mod_positive(mod_positive(u_pixel_coord_upper, pattern_size) * 256.0, pattern_size) * 256.0 + u_pixel_coord_lower, pattern_size);
            return (u_tile_units_to_pixels * pos + offset) / pattern_size;
        }

        Varyings main(const Attributes attrs) {
            Varyings varyings;
            float4 projected = u_matrix * float4(attrs.a_pos, 0.0, 1.0);
            float inv_w = projected.w == 0.0 ? 1.0 : 1.0 / projected.w;
            float2 ndc = projected.xy * inv_w;
            varyings.position = float2((ndc.x * 0.5 + 0.5) * u_viewport.x,
                                       (0.5 - ndc.y * 0.5) * u_viewport.y);
            varyings.pos_a = get_pattern_pos(u_scale_a * u_pattern_size_a, attrs.a_pos);
            varyings.pos_b = get_pattern_pos(u_scale_b * u_pattern_size_b, attrs.a_pos);
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        uniform shader u_image;
        uniform float2 u_pattern_tl_a;
        uniform float2 u_pattern_br_a;
        uniform float2 u_pattern_tl_b;
        uniform float2 u_pattern_br_b;
        uniform float u_mix;
        uniform float u_opacity;

        float2 mod_positive(float2 x, float y) {
            return x - y * floor(x / y);
        }

        float2 mix2(float2 a, float2 b, float2 t) {
            return a + (b - a) * t;
        }

        float2 main(const Varyings varyings, out half4 color) {
            float2 imagecoord_a = mod_positive(varyings.pos_a, 1.0);
            float2 imagecoord_b = mod_positive(varyings.pos_b, 1.0);
            float4 color_a = u_image.eval(mix2(u_pattern_tl_a, u_pattern_br_a, imagecoord_a));
            float4 color_b = u_image.eval(mix2(u_pattern_tl_b, u_pattern_br_b, imagecoord_b));
            color = half4(mix(color_a, color_b, u_mix) * u_opacity);
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                    sizeof(MeshVertex),
                                                    varyings,
                                                    vertexShader,
                                                    fragmentShader,
                                                    SkColorSpace::MakeSRGB(),
                                                    kPremul_SkAlphaType);
    (void)error;
    specification = std::move(spec);
    return specification;
}

void writeUniform(sk_sp<SkData>& uniforms,
                  const SkMeshSpecification& specification,
                  const char* name,
                  const void* data,
                  const std::size_t size) {
    const auto* uniform = specification.findUniform(name);
    std::string indexedName;
    if (!uniform) {
        indexedName = std::string(name) + "[0]";
        uniform = specification.findUniform(indexedName.c_str());
    }
    if (!uniform || uniform->offset + size > uniforms->size()) {
        return;
    }
    std::memcpy(static_cast<std::uint8_t*>(uniforms->writable_data()) + uniform->offset, data, size);
}

std::array<float, 2> projectToScreen(const std::array<float, 16>& matrix,
                                     const float viewport[2],
                                     const float x,
                                     const float y) {
    const auto clipX = matrix[0] * x + matrix[4] * y + matrix[12];
    const auto clipY = matrix[1] * x + matrix[5] * y + matrix[13];
    const auto clipW = matrix[3] * x + matrix[7] * y + matrix[15];
    const auto invW = clipW == 0.0f ? 1.0f : 1.0f / clipW;
    const auto ndcX = clipX * invW;
    const auto ndcY = clipY * invW;
    return {(ndcX * 0.5f + 0.5f) * viewport[0], (0.5f - ndcY * 0.5f) * viewport[1]};
}

std::array<float, 2> collisionPointToScreen(const std::array<float, 16>& matrix,
                                            const float viewport[2],
                                            const float cameraToCenterDistance,
                                            const std::array<float, 2>& extrudeScale,
                                            const MeshVertex& vertex) {
    const auto anchorW = matrix[3] * vertex.collisionAnchorPos[0] +
                         matrix[7] * vertex.collisionAnchorPos[1] + matrix[15];
    const auto ratio = std::clamp(0.5f + 0.5f * (cameraToCenterDistance / anchorW), 0.0f, 4.0f);

    auto clipX = matrix[0] * vertex.position[0] + matrix[4] * vertex.position[1] + matrix[12];
    auto clipY = matrix[1] * vertex.position[0] + matrix[5] * vertex.position[1] + matrix[13];
    const auto clipW = matrix[3] * vertex.position[0] + matrix[7] * vertex.position[1] + matrix[15];
    clipX += (vertex.collisionExtrude[0] + vertex.collisionShift[0]) * extrudeScale[0] * clipW * ratio;
    clipY += (vertex.collisionExtrude[1] + vertex.collisionShift[1]) * extrudeScale[1] * clipW * ratio;

    const auto invW = clipW == 0.0f ? 1.0f : 1.0f / clipW;
    const auto ndcX = clipX * invW;
    const auto ndcY = clipY * invW;
    return {(ndcX * 0.5f + 0.5f) * viewport[0], (0.5f - ndcY * 0.5f) * viewport[1]};
}

} // namespace

Drawable::Drawable(std::string name)
    : gfx::Drawable(std::move(name)),
      uniformBuffers(std::make_unique<UniformBufferArray>()) {}

void Drawable::draw(PaintParameters& parameters) const {
    draw(parameters, nullptr);
}

void Drawable::draw(PaintParameters& parameters, const gfx::UniformBufferArray* layerUniforms) const {
    if (!getEnabled() || !sharedIndexes || !parameters.renderPass) {
        return;
    }

    auto* renderPass = static_cast<RenderPass*>(parameters.renderPass.get());
    auto* canvas = renderPass->getCanvas();
    if (!canvas) {
        return;
    }

    auto matrix = identityMatrix();
    auto color = SkColor4f{1.0f, 0.0f, 1.0f, 1.0f};
    float colorT = 0.0f;
    float opacityT = 0.0f;
    bool fillExtrusionDrawable = false;
    bool fillExtrusionPatternDrawable = false;
    float fillExtrusionBase = 0.0f;
    float fillExtrusionHeight = 0.0f;
    float fillExtrusionHeightFactor = 1.0f;
    float fillExtrusionBaseT = 0.0f;
    float fillExtrusionHeightT = 0.0f;
    float fillExtrusionOpacity = 1.0f;
    std::array<float, 3> fillExtrusionLightColor = {1.0f, 1.0f, 1.0f};
    std::array<float, 3> fillExtrusionLightPosition = {0.0f, 0.0f, 1.0f};
    float fillExtrusionLightIntensity = 0.5f;
    float fillExtrusionVerticalGradient = 1.0f;
    bool fillOutlineDrawable = false;
    float lineWidth = 1.0f;
    float lineGapWidth = 0.0f;
    float lineBlur = 0.0f;
    float lineOffset = 0.0f;
    float lineFloorWidth = 1.0f;
    float lineWidthT = 0.0f;
    float lineGapWidthT = 0.0f;
    float lineBlurT = 0.0f;
    float lineOffsetT = 0.0f;
    float lineFloorWidthT = 0.0f;
    float lineRatio = 1.0f;
    bool lineDrawable = false;
    bool circleDrawable = false;
    float circleColorT = 0.0f;
    float circleRadius = 1.0f;
    float circleRadiusT = 0.0f;
    float circleBlur = 0.0f;
    float circleBlurT = 0.0f;
    float circleOpacity = 1.0f;
    float circleOpacityT = 0.0f;
    auto circleColor = SkColor4f{0.0f, 0.0f, 0.0f, 1.0f};
    auto circleStrokeColor = SkColor4f{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 2> circleExtrudeScale = {1.0f, 1.0f};
    float circleScaleWithMap = 0.0f;
    float circlePitchWithMap = 0.0f;
    float circleStrokeColorT = 0.0f;
    float circleStrokeWidth = 0.0f;
    float circleStrokeWidthT = 0.0f;
    float circleStrokeOpacity = 1.0f;
    float circleStrokeOpacityT = 0.0f;
    bool rasterDrawable = false;
    bool colorReliefDrawable = false;
    bool heatmapDrawable = false;
    bool heatmapTextureDrawable = false;
    bool symbolIconDrawable = false;
    bool symbolSDFDrawable = false;
    bool symbolTextAndIconDrawable = false;
    bool collisionBoxDrawable = false;
    bool collisionCircleDrawable = false;
    std::array<float, 2> collisionExtrudeScale = {1.0f, 1.0f};
    float collisionOverscaleFactor = 1.0f;
    bool hillshadePrepareDrawable = false;
    bool hillshadeDrawable = false;
    std::array<float, 4> colorReliefUnpack = {1.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 2> colorReliefDimension = {1.0f, 1.0f};
    int32_t colorReliefRampSize = 0;
    float colorReliefOpacity = 1.0f;
    float heatmapExtrudeScale = 1.0f;
    float heatmapWeight = 1.0f;
    float heatmapRadius = 30.0f;
    float heatmapIntensity = 1.0f;
    float heatmapWeightT = 0.0f;
    float heatmapRadiusT = 0.0f;
    float heatmapTextureOpacity = 1.0f;
    std::array<float, 16> symbolLabelPlaneMatrix = identityMatrix();
    std::array<float, 16> symbolCoordMatrix = identityMatrix();
    std::array<float, 2> symbolTexsize = {1.0f, 1.0f};
    float symbolRotateSymbol = 0.0f;
    float symbolPitchWithMap = 0.0f;
    float symbolIsSizeZoomConstant = 1.0f;
    float symbolIsSizeFeatureConstant = 1.0f;
    float symbolIsOffset = 0.0f;
    float symbolSizeT = 0.0f;
    float symbolSize = 1.0f;
    float symbolOpacityT = 0.0f;
    float symbolOpacity = 1.0f;
    float symbolIsText = 0.0f;
    float symbolIsHalo = 0.0f;
    float symbolGammaScale = 1.0f;
    float symbolFillColorT = 0.0f;
    float symbolHaloColorT = 0.0f;
    float symbolHaloWidthT = 0.0f;
    float symbolHaloBlurT = 0.0f;
    SkColor4f symbolFillColor = {0.0f, 0.0f, 0.0f, 1.0f};
    SkColor4f symbolHaloColor = {0.0f, 0.0f, 0.0f, 1.0f};
    float symbolHaloWidth = 0.0f;
    float symbolHaloBlur = 0.0f;
    std::array<float, 4> hillshadeUnpack = {1.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 2> hillshadeDimension = {1.0f, 1.0f};
    float hillshadeZoom = 0.0f;
    bool hillshadeOverzoom = false;
    std::array<float, 2> hillshadeLatrange = {0.0f, 0.0f};
    float hillshadeExaggeration = 0.5f;
    int32_t hillshadeMethod = 0;
    int32_t hillshadeNumLights = 1;
    std::array<float, 4> hillshadeAccent = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 4> hillshadeAltitudes = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 4> hillshadeAzimuths = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 16> hillshadeShadows = {};
    std::array<float, 16> hillshadeHighlights = {};
    std::array<float, 4> rasterSpinWeights = {1.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 2> rasterTlParent = {0.0f, 0.0f};
    float rasterScaleParent = 1.0f;
    float rasterBufferScale = 1.0f;
    float rasterFadeT = 0.0f;
    float rasterOpacity = 1.0f;
    float rasterBrightnessLow = 0.0f;
    float rasterBrightnessHigh = 1.0f;
    float rasterSaturationFactor = 0.0f;
    float rasterContrastFactor = 1.0f;
    bool fillPatternDrawable = false;
    bool fillOutlinePatternDrawable = false;
    std::array<float, 2> fillPatternPixelCoordUpper = {0.0f, 0.0f};
    std::array<float, 2> fillPatternPixelCoordLower = {0.0f, 0.0f};
    std::array<float, 4> fillPatternFrom = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 4> fillPatternTo = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 2> fillPatternTexsize = {1.0f, 1.0f};
    float fillPatternTileRatio = 1.0f;
    float fillPatternFade = 0.0f;
    float fillPatternFromScale = 1.0f;
    float fillPatternToScale = 1.0f;
    bool backgroundPatternDrawable = false;
    std::array<float, 2> backgroundPatternPixelCoordUpper = {0.0f, 0.0f};
    std::array<float, 2> backgroundPatternPixelCoordLower = {0.0f, 0.0f};
    std::array<float, 2> backgroundPatternTlA = {0.0f, 0.0f};
    std::array<float, 2> backgroundPatternBrA = {0.0f, 0.0f};
    std::array<float, 2> backgroundPatternTlB = {0.0f, 0.0f};
    std::array<float, 2> backgroundPatternBrB = {0.0f, 0.0f};
    std::array<float, 2> backgroundPatternSizeA = {1.0f, 1.0f};
    std::array<float, 2> backgroundPatternSizeB = {1.0f, 1.0f};
    float backgroundPatternTileUnitsToPixels = 1.0f;
    float backgroundPatternScaleA = 1.0f;
    float backgroundPatternScaleB = 1.0f;
    float backgroundPatternMix = 0.0f;
    float backgroundPatternOpacity = 1.0f;
    const auto& lineImageTexture = getTexture(shaders::idLineImageTexture);
    const auto& fillImageTexture = getTexture(shaders::idFillImageTexture);
    const auto& fillExtrusionImageTexture = getTexture(shaders::idFillExtrusionImageTexture);
    const auto& backgroundImageTexture = getTexture(shaders::idBackgroundImageTexture);
    const auto& colorReliefImageTexture = getTexture(shaders::idColorReliefImageTexture);
    const auto& colorReliefElevationStopsTexture = getTexture(shaders::idColorReliefElevationStopsTexture);
    const auto& colorReliefColorStopsTexture = getTexture(shaders::idColorReliefColorStopsTexture);
    const auto& heatmapImageTexture = getTexture(shaders::idHeatmapImageTexture);
    const auto& heatmapColorRampTexture = getTexture(shaders::idHeatmapColorRampTexture);
    const auto& hillshadeImageTexture = getTexture(shaders::idHillshadeImageTexture);
    const auto& rasterImage0Texture = getTexture(shaders::idRasterImage0Texture);
    const auto& rasterImage1Texture = getTexture(shaders::idRasterImage1Texture);
    const auto& symbolImageTexture = getTexture(shaders::idSymbolImageTexture);
    const auto& symbolImageIconTexture = getTexture(shaders::idSymbolImageIconTexture);
    const bool lineGradientDrawable = getName().find("lineGradient") != std::string::npos;
    const bool linePatternDrawable = getName().find("linePattern") != std::string::npos;
    const bool lineSDFDrawable = getName().find("lineSDF") != std::string::npos;
    bool hasFillExtrusionPositionAttribute = false;
    bool hasLinePositionAttribute = vertexDataType == gfx::AttributeDataType::Short4;
    if (const auto& attrs = getVertexAttributes()) {
        if (const auto& attr = attrs->get(shaders::idFillExtrusionPosVertexAttribute)) {
            hasFillExtrusionPositionAttribute = attr->getSharedType() == gfx::AttributeDataType::Short2 ||
                                                attr->getDataType() == gfx::AttributeDataType::Short2;
        }
        if (const auto& lineDataAttr = attrs->get(shaders::idLineDataVertexAttribute)) {
            const auto& linePosAttr = attrs->get(shaders::idLinePosNormalVertexAttribute);
            const auto hasLineData = lineDataAttr->getSharedType() == gfx::AttributeDataType::UByte4 ||
                                     lineDataAttr->getDataType() == gfx::AttributeDataType::UByte4;
            const auto hasLinePos = linePosAttr && (linePosAttr->getSharedType() == gfx::AttributeDataType::Short2 ||
                                                     linePosAttr->getDataType() == gfx::AttributeDataType::Short2 ||
                                                     linePosAttr->getSharedType() == gfx::AttributeDataType::Short4 ||
                                                     linePosAttr->getDataType() == gfx::AttributeDataType::Short4);
            hasLinePositionAttribute = hasLineData && (hasLinePositionAttribute || hasLinePos);
        }
    }
    if (getName().find("-collision/") != std::string::npos) {
        collisionBoxDrawable = getName().find("/box") != std::string::npos;
        collisionCircleDrawable = getName().find("/circle") != std::string::npos;
        if (const auto* drawableUBO = getUBO<shaders::CollisionDrawableUBO>(&getUniformBuffers(), shaders::idCollisionDrawableUBO)) {
            matrix = drawableUBO->matrix;
        }
        if (const auto* tilePropsUBO = getUBO<shaders::CollisionTilePropsUBO>(&getUniformBuffers(), shaders::idCollisionTilePropsUBO)) {
            collisionExtrudeScale = tilePropsUBO->extrude_scale;
            collisionOverscaleFactor = tilePropsUBO->overscale_factor;
        }
    } else if (getName().find("/icon") != std::string::npos && symbolImageTexture) {
        const shaders::SymbolDrawableUBO* drawableUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        drawableUBO = getUBO<shaders::SymbolDrawableUBO>(layerUniforms, shaders::idSymbolDrawableUBO, getUBOIndex());
#endif
        if (!drawableUBO) {
            drawableUBO = getUBO<shaders::SymbolDrawableUBO>(&getUniformBuffers(), shaders::idSymbolDrawableUBO);
        }
        if (drawableUBO) {
            symbolIconDrawable = true;
            matrix = drawableUBO->matrix;
            symbolLabelPlaneMatrix = drawableUBO->label_plane_matrix;
            symbolCoordMatrix = drawableUBO->coord_matrix;
            symbolTexsize = drawableUBO->texsize;
            symbolRotateSymbol = drawableUBO->rotate_symbol ? 1.0f : 0.0f;
            symbolPitchWithMap = drawableUBO->pitch_with_map ? 1.0f : 0.0f;
            symbolIsSizeZoomConstant = drawableUBO->is_size_zoom_constant ? 1.0f : 0.0f;
            symbolIsSizeFeatureConstant = drawableUBO->is_size_feature_constant ? 1.0f : 0.0f;
            symbolIsOffset = drawableUBO->is_offset ? 1.0f : 0.0f;
            symbolSizeT = drawableUBO->size_t;
            symbolSize = drawableUBO->size;
            symbolOpacityT = drawableUBO->opacity_t;
            if (const auto* props = getUBO<shaders::SymbolEvaluatedPropsUBO>(layerUniforms, shaders::idSymbolEvaluatedPropsUBO)) {
                symbolOpacity = props->icon_opacity;
            }
        }
    } else if (symbolImageTexture || symbolImageIconTexture) {
        const shaders::SymbolDrawableUBO* drawableUBO = nullptr;
        const shaders::SymbolTilePropsUBO* tilePropsUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        drawableUBO = getUBO<shaders::SymbolDrawableUBO>(layerUniforms, shaders::idSymbolDrawableUBO, getUBOIndex());
        tilePropsUBO = getUBO<shaders::SymbolTilePropsUBO>(layerUniforms, shaders::idSymbolTilePropsUBO, getUBOIndex());
#endif
        if (!drawableUBO) {
            drawableUBO = getUBO<shaders::SymbolDrawableUBO>(&getUniformBuffers(), shaders::idSymbolDrawableUBO);
        }
        if (!tilePropsUBO) {
            tilePropsUBO = getUBO<shaders::SymbolTilePropsUBO>(&getUniformBuffers(), shaders::idSymbolTilePropsUBO);
        }
        if (drawableUBO && tilePropsUBO) {
            symbolTextAndIconDrawable = symbolImageTexture && symbolImageIconTexture;
            symbolSDFDrawable = !symbolTextAndIconDrawable;
            matrix = drawableUBO->matrix;
            symbolLabelPlaneMatrix = drawableUBO->label_plane_matrix;
            symbolCoordMatrix = drawableUBO->coord_matrix;
            symbolTexsize = drawableUBO->texsize;
            symbolRotateSymbol = drawableUBO->rotate_symbol ? 1.0f : 0.0f;
            symbolPitchWithMap = drawableUBO->pitch_with_map ? 1.0f : 0.0f;
            symbolIsSizeZoomConstant = drawableUBO->is_size_zoom_constant ? 1.0f : 0.0f;
            symbolIsSizeFeatureConstant = drawableUBO->is_size_feature_constant ? 1.0f : 0.0f;
            symbolIsOffset = drawableUBO->is_offset ? 1.0f : 0.0f;
            symbolSizeT = drawableUBO->size_t;
            symbolSize = drawableUBO->size;
            symbolFillColorT = drawableUBO->fill_color_t;
            symbolHaloColorT = drawableUBO->halo_color_t;
            symbolOpacityT = drawableUBO->opacity_t;
            symbolHaloWidthT = drawableUBO->halo_width_t;
            symbolHaloBlurT = drawableUBO->halo_blur_t;
            symbolIsText = tilePropsUBO->is_text ? 1.0f : 0.0f;
            symbolIsHalo = tilePropsUBO->is_halo ? 1.0f : 0.0f;
            symbolGammaScale = tilePropsUBO->gamma_scale;
            if (const auto* props = getUBO<shaders::SymbolEvaluatedPropsUBO>(layerUniforms, shaders::idSymbolEvaluatedPropsUBO)) {
                symbolFillColor = symbolIsText >= 0.5f ? toRawSkColor(props->text_fill_color)
                                                       : toRawSkColor(props->icon_fill_color);
                symbolHaloColor = symbolIsText >= 0.5f ? toRawSkColor(props->text_halo_color)
                                                       : toRawSkColor(props->icon_halo_color);
                symbolOpacity = symbolIsText >= 0.5f ? props->text_opacity : props->icon_opacity;
                symbolHaloWidth = symbolIsText >= 0.5f ? props->text_halo_width : props->icon_halo_width;
                symbolHaloBlur = symbolIsText >= 0.5f ? props->text_halo_blur : props->icon_halo_blur;
            }
        }
    } else if (getName().find("heatmapTexture") != std::string::npos && heatmapImageTexture && heatmapColorRampTexture) {
        if (const auto* props = getUBO<shaders::HeatmapTexturePropsUBO>(layerUniforms, shaders::idHeatmapTexturePropsUBO)) {
            heatmapTextureDrawable = true;
            matrix = props->matrix;
            heatmapTextureOpacity = props->opacity;
        }
    } else if (getName().find("heatmap") != std::string::npos && getName().find("Texture") == std::string::npos) {
        const shaders::HeatmapDrawableUBO* drawableUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        drawableUBO = getUBO<shaders::HeatmapDrawableUBO>(layerUniforms, shaders::idHeatmapDrawableUBO, getUBOIndex());
#endif
        if (!drawableUBO) {
            drawableUBO = getUBO<shaders::HeatmapDrawableUBO>(&getUniformBuffers(), shaders::idHeatmapDrawableUBO);
        }
        if (drawableUBO) {
            heatmapDrawable = true;
            matrix = drawableUBO->matrix;
            heatmapExtrudeScale = drawableUBO->extrude_scale;
            heatmapWeightT = drawableUBO->weight_t;
            heatmapRadiusT = drawableUBO->radius_t;
            if (const auto* props = getUBO<shaders::HeatmapEvaluatedPropsUBO>(layerUniforms, shaders::idHeatmapEvaluatedPropsUBO)) {
                heatmapWeight = props->weight;
                heatmapRadius = props->radius;
                heatmapIntensity = props->intensity;
            }
        }
    } else if (getName().find("hillshadePrepare") != std::string::npos && hillshadeImageTexture) {
        const shaders::HillshadePrepareDrawableUBO* drawableUBO = nullptr;
        const shaders::HillshadePrepareTilePropsUBO* tilePropsUBO = nullptr;
        drawableUBO = getUBO<shaders::HillshadePrepareDrawableUBO>(&getUniformBuffers(), shaders::idHillshadePrepareDrawableUBO);
        tilePropsUBO = getUBO<shaders::HillshadePrepareTilePropsUBO>(&getUniformBuffers(), shaders::idHillshadePrepareTilePropsUBO);
        if (drawableUBO && tilePropsUBO) {
            hillshadePrepareDrawable = true;
            matrix = drawableUBO->matrix;
            hillshadeUnpack = tilePropsUBO->unpack;
            hillshadeDimension = tilePropsUBO->dimension;
            hillshadeZoom = tilePropsUBO->zoom;
        }
    } else if (getName().find("hillshade") != std::string::npos && hillshadeImageTexture) {
        const shaders::HillshadeDrawableUBO* drawableUBO = nullptr;
        const shaders::HillshadeTilePropsUBO* tilePropsUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        drawableUBO = getUBO<shaders::HillshadeDrawableUBO>(layerUniforms, shaders::idHillshadeDrawableUBO, getUBOIndex());
        tilePropsUBO = getUBO<shaders::HillshadeTilePropsUBO>(layerUniforms, shaders::idHillshadeTilePropsUBO, getUBOIndex());
#endif
        if (!drawableUBO) {
            drawableUBO = getUBO<shaders::HillshadeDrawableUBO>(&getUniformBuffers(), shaders::idHillshadeDrawableUBO);
        }
        if (!tilePropsUBO) {
            tilePropsUBO = getUBO<shaders::HillshadeTilePropsUBO>(&getUniformBuffers(), shaders::idHillshadeTilePropsUBO);
        }
        if (drawableUBO && tilePropsUBO) {
            hillshadeDrawable = true;
            matrix = drawableUBO->matrix;
            hillshadeLatrange = tilePropsUBO->latrange;
            hillshadeExaggeration = tilePropsUBO->exaggeration;
            hillshadeMethod = tilePropsUBO->method;
            hillshadeNumLights = tilePropsUBO->num_lights;
            if (const auto* data = static_cast<const gfx::HillshadePrepareDrawableData*>(getData().get())) {
                hillshadeUnpack = hillshadeUnpackVector(data->encoding);
                hillshadeDimension = {static_cast<float>(data->stride), static_cast<float>(data->stride)};
                hillshadeZoom = static_cast<float>(std::min<int32_t>(getTileID() ? getTileID()->canonical.z : data->maxzoom,
                                                                     data->maxzoom));
                hillshadeOverzoom = parameters.state.getZoom() > static_cast<double>(data->maxzoom);
            }
            if (getTileID() && !getData()) {
                hillshadeZoom = static_cast<float>(getTileID()->canonical.z);
            }
            if (const auto* props = getUBO<shaders::HillshadeEvaluatedPropsUBO>(layerUniforms, shaders::idHillshadeEvaluatedPropsUBO)) {
                hillshadeAccent = {props->accent.r, props->accent.g, props->accent.b, props->accent.a};
                hillshadeAltitudes = props->altitudes;
                hillshadeAzimuths = props->azimuths;
                hillshadeShadows = props->shadows;
                hillshadeHighlights = props->highlights;
            }
        }
    } else if (colorReliefImageTexture) {
        const shaders::ColorReliefDrawableUBO* drawableUBO = nullptr;
        const shaders::ColorReliefTilePropsUBO* tilePropsUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        drawableUBO = getUBO<shaders::ColorReliefDrawableUBO>(
            layerUniforms, shaders::idColorReliefDrawableUBO, getUBOIndex());
        tilePropsUBO = getUBO<shaders::ColorReliefTilePropsUBO>(
            layerUniforms, shaders::idColorReliefTilePropsUBO, getUBOIndex());
#endif
        if (!drawableUBO) {
            drawableUBO = getUBO<shaders::ColorReliefDrawableUBO>(&getUniformBuffers(), shaders::idColorReliefDrawableUBO);
        }
        if (!tilePropsUBO) {
            tilePropsUBO = getUBO<shaders::ColorReliefTilePropsUBO>(&getUniformBuffers(), shaders::idColorReliefTilePropsUBO);
        }
        if (drawableUBO && tilePropsUBO) {
            colorReliefDrawable = true;
            matrix = drawableUBO->matrix;
            colorReliefUnpack = tilePropsUBO->unpack;
            colorReliefDimension = tilePropsUBO->dimension;
            colorReliefRampSize = tilePropsUBO->color_ramp_size;
            if (const auto* props = getUBO<shaders::ColorReliefEvaluatedPropsUBO>(layerUniforms, shaders::idColorReliefEvaluatedPropsUBO)) {
                colorReliefOpacity = props->opacity;
            }
        }
    } else if (hasFillExtrusionPositionAttribute && !hasLinePositionAttribute) {
        if (getName().find("/depth") != std::string::npos) {
            return;
        }
        const shaders::FillExtrusionDrawableUBO* extrusionDrawableUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        extrusionDrawableUBO = getUBO<shaders::FillExtrusionDrawableUBO>(
            layerUniforms, shaders::idFillExtrusionDrawableUBO, getUBOIndex());
#endif
        if (!extrusionDrawableUBO) {
            extrusionDrawableUBO = getUBO<shaders::FillExtrusionDrawableUBO>(&getUniformBuffers(), shaders::idFillExtrusionDrawableUBO);
        }
        if (extrusionDrawableUBO) {
            fillExtrusionDrawable = true;
            fillExtrusionPatternDrawable = static_cast<bool>(fillExtrusionImageTexture);
            matrix = extrusionDrawableUBO->matrix;
            fillPatternPixelCoordUpper = extrusionDrawableUBO->pixel_coord_upper;
            fillPatternPixelCoordLower = extrusionDrawableUBO->pixel_coord_lower;
            fillPatternTileRatio = extrusionDrawableUBO->tile_ratio;
            fillExtrusionHeightFactor = extrusionDrawableUBO->height_factor;
            fillExtrusionBaseT = extrusionDrawableUBO->base_t;
            fillExtrusionHeightT = extrusionDrawableUBO->height_t;
            colorT = extrusionDrawableUBO->color_t;
            if (const auto* props = getUBO<shaders::FillExtrusionPropsUBO>(layerUniforms, shaders::idFillExtrusionPropsUBO)) {
                color = toRawSkColor(props->color);
                fillExtrusionBase = props->base;
                fillExtrusionHeight = props->height;
                fillExtrusionOpacity = props->opacity;
                fillExtrusionLightColor = props->light_color;
                fillExtrusionLightPosition = props->light_position;
                fillExtrusionLightIntensity = props->light_intensity;
                fillExtrusionVerticalGradient = props->vertical_gradient;
                fillPatternFade = props->fade;
                fillPatternFromScale = props->from_scale;
                fillPatternToScale = props->to_scale;
            }
            if (fillExtrusionPatternDrawable) {
                const auto* tileProps = getUBO<shaders::FillExtrusionTilePropsUBO>(
                    layerUniforms, shaders::idFillExtrusionTilePropsUBO, getUBOIndex());
                if (!tileProps) {
                    tileProps = getUBO<shaders::FillExtrusionTilePropsUBO>(&getUniformBuffers(), shaders::idFillExtrusionTilePropsUBO);
                }
                if (tileProps) {
                    fillPatternFrom = tileProps->pattern_from;
                    fillPatternTo = tileProps->pattern_to;
                    fillPatternTexsize = tileProps->texsize;
                }
            }
        }
    } else if (getName().find("raster") != std::string::npos) {
        const shaders::RasterDrawableUBO* rasterDrawableUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        rasterDrawableUBO = getUBO<shaders::RasterDrawableUBO>(layerUniforms, shaders::idRasterDrawableUBO, getUBOIndex());
#endif
        if (!rasterDrawableUBO) {
            rasterDrawableUBO = getUBO<shaders::RasterDrawableUBO>(&getUniformBuffers(), shaders::idRasterDrawableUBO);
        }
        if (rasterDrawableUBO) {
            rasterDrawable = true;
            matrix = rasterDrawableUBO->matrix;
            if (const auto* props = getUBO<shaders::RasterEvaluatedPropsUBO>(layerUniforms, shaders::idRasterEvaluatedPropsUBO)) {
                rasterSpinWeights = props->spin_weights;
                rasterTlParent = props->tl_parent;
                rasterScaleParent = props->scale_parent;
                rasterBufferScale = props->buffer_scale;
                rasterFadeT = props->fade_t;
                rasterOpacity = props->opacity;
                rasterBrightnessLow = props->brightness_low;
                rasterBrightnessHigh = props->brightness_high;
                rasterSaturationFactor = props->saturation_factor;
                rasterContrastFactor = props->contrast_factor;
            }
        }
    } else if (getName().find("circle") != std::string::npos) {
        const shaders::CircleDrawableUBO* circleDrawableUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        circleDrawableUBO = getUBO<shaders::CircleDrawableUBO>(layerUniforms, shaders::idCircleDrawableUBO, getUBOIndex());
#endif
        if (!circleDrawableUBO) {
            circleDrawableUBO = getUBO<shaders::CircleDrawableUBO>(&getUniformBuffers(), shaders::idCircleDrawableUBO);
        }
        if (circleDrawableUBO) {
            circleDrawable = true;
            matrix = circleDrawableUBO->matrix;
            circleExtrudeScale = circleDrawableUBO->extrude_scale;
            circleColorT = circleDrawableUBO->color_t;
            circleRadiusT = circleDrawableUBO->radius_t;
            circleBlurT = circleDrawableUBO->blur_t;
            circleOpacityT = circleDrawableUBO->opacity_t;
            circleStrokeColorT = circleDrawableUBO->stroke_color_t;
            circleStrokeWidthT = circleDrawableUBO->stroke_width_t;
            circleStrokeOpacityT = circleDrawableUBO->stroke_opacity_t;
            if (const auto* props = getUBO<shaders::CircleEvaluatedPropsUBO>(layerUniforms, shaders::idCircleEvaluatedPropsUBO)) {
                circleColor = toRawSkColor(props->color);
                circleStrokeColor = toRawSkColor(props->stroke_color);
                circleRadius = props->radius;
                circleBlur = props->blur;
                circleOpacity = props->opacity;
                circleStrokeWidth = props->stroke_width;
                circleStrokeOpacity = props->stroke_opacity;
                circleScaleWithMap = props->scale_with_map ? 1.0f : 0.0f;
                circlePitchWithMap = props->pitch_with_map ? 1.0f : 0.0f;
            }
        }
    } else if (hasLinePositionAttribute) {
        const shaders::LineDrawableUBO* lineDrawableUBO = nullptr;
        shaders::LineDrawableUBO gradientAsLineDrawableUBO = {};
        const auto isFillOutlineTriangulated = getName().find("fill-outline") != std::string::npos &&
                                               getName().find("pattern") == std::string::npos;
        if (isFillOutlineTriangulated) {
            const shaders::FillOutlineTriangulatedDrawableUBO* outlineTriangulatedUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
            if (const auto* fillDrawableUnion = getUBO<shaders::FillDrawableUnionUBO>(layerUniforms, shaders::idFillDrawableUBO, getUBOIndex())) {
                outlineTriangulatedUBO = &fillDrawableUnion->fillOutlineTriangulatedDrawableUBO;
            }
#endif
            if (!outlineTriangulatedUBO) {
                outlineTriangulatedUBO = getUBO<shaders::FillOutlineTriangulatedDrawableUBO>(&getUniformBuffers(), shaders::idFillDrawableUBO);
            }
            if (outlineTriangulatedUBO) {
                gradientAsLineDrawableUBO.matrix = outlineTriangulatedUBO->matrix;
                gradientAsLineDrawableUBO.ratio = outlineTriangulatedUBO->ratio;
                gradientAsLineDrawableUBO.blur_t = 0.0f;
                gradientAsLineDrawableUBO.opacity_t = 0.0f;
                gradientAsLineDrawableUBO.gapwidth_t = 0.0f;
                gradientAsLineDrawableUBO.offset_t = 0.0f;
                gradientAsLineDrawableUBO.width_t = 0.0f;
                lineDrawableUBO = &gradientAsLineDrawableUBO;
            }
        }
#if MLN_UBO_CONSOLIDATION
        if (!lineDrawableUBO) {
            if (const auto* lineDrawableUnion = getUBO<shaders::LineDrawableUnionUBO>(
                    layerUniforms, shaders::idLineDrawableUBO, getUBOIndex())) {
            if (lineGradientDrawable) {
                const auto& gradientUBO = lineDrawableUnion->lineGradientDrawableUBO;
                gradientAsLineDrawableUBO.matrix = gradientUBO.matrix;
                gradientAsLineDrawableUBO.ratio = gradientUBO.ratio;
                gradientAsLineDrawableUBO.blur_t = gradientUBO.blur_t;
                gradientAsLineDrawableUBO.opacity_t = gradientUBO.opacity_t;
                gradientAsLineDrawableUBO.gapwidth_t = gradientUBO.gapwidth_t;
                gradientAsLineDrawableUBO.offset_t = gradientUBO.offset_t;
                gradientAsLineDrawableUBO.width_t = gradientUBO.width_t;
                lineDrawableUBO = &gradientAsLineDrawableUBO;
            } else if (linePatternDrawable) {
                const auto& patternUBO = lineDrawableUnion->linePatternDrawableUBO;
                gradientAsLineDrawableUBO.matrix = patternUBO.matrix;
                gradientAsLineDrawableUBO.ratio = patternUBO.ratio;
                gradientAsLineDrawableUBO.blur_t = patternUBO.blur_t;
                gradientAsLineDrawableUBO.opacity_t = patternUBO.opacity_t;
                gradientAsLineDrawableUBO.gapwidth_t = patternUBO.gapwidth_t;
                gradientAsLineDrawableUBO.offset_t = patternUBO.offset_t;
                gradientAsLineDrawableUBO.width_t = patternUBO.width_t;
                lineDrawableUBO = &gradientAsLineDrawableUBO;
            } else if (lineSDFDrawable) {
                const auto& sdfUBO = lineDrawableUnion->lineSDFDrawableUBO;
                gradientAsLineDrawableUBO.matrix = sdfUBO.matrix;
                gradientAsLineDrawableUBO.ratio = sdfUBO.ratio;
                gradientAsLineDrawableUBO.color_t = sdfUBO.color_t;
                gradientAsLineDrawableUBO.blur_t = sdfUBO.blur_t;
                gradientAsLineDrawableUBO.opacity_t = sdfUBO.opacity_t;
                gradientAsLineDrawableUBO.gapwidth_t = sdfUBO.gapwidth_t;
                gradientAsLineDrawableUBO.offset_t = sdfUBO.offset_t;
                gradientAsLineDrawableUBO.width_t = sdfUBO.width_t;
                lineFloorWidthT = sdfUBO.floorwidth_t;
                lineDrawableUBO = &gradientAsLineDrawableUBO;
            } else {
                lineDrawableUBO = &lineDrawableUnion->lineDrawableUBO;
            }
            }
        }
#endif
        if (!lineDrawableUBO) {
            if (lineGradientDrawable) {
                if (const auto* gradientUBO = getUBO<shaders::LineGradientDrawableUBO>(&getUniformBuffers(), shaders::idLineDrawableUBO)) {
                    gradientAsLineDrawableUBO.matrix = gradientUBO->matrix;
                    gradientAsLineDrawableUBO.ratio = gradientUBO->ratio;
                    gradientAsLineDrawableUBO.blur_t = gradientUBO->blur_t;
                    gradientAsLineDrawableUBO.opacity_t = gradientUBO->opacity_t;
                    gradientAsLineDrawableUBO.gapwidth_t = gradientUBO->gapwidth_t;
                    gradientAsLineDrawableUBO.offset_t = gradientUBO->offset_t;
                    gradientAsLineDrawableUBO.width_t = gradientUBO->width_t;
                    lineDrawableUBO = &gradientAsLineDrawableUBO;
                }
            } else if (linePatternDrawable) {
                if (const auto* patternUBO = getUBO<shaders::LinePatternDrawableUBO>(&getUniformBuffers(), shaders::idLineDrawableUBO)) {
                    gradientAsLineDrawableUBO.matrix = patternUBO->matrix;
                    gradientAsLineDrawableUBO.ratio = patternUBO->ratio;
                    gradientAsLineDrawableUBO.blur_t = patternUBO->blur_t;
                    gradientAsLineDrawableUBO.opacity_t = patternUBO->opacity_t;
                    gradientAsLineDrawableUBO.gapwidth_t = patternUBO->gapwidth_t;
                    gradientAsLineDrawableUBO.offset_t = patternUBO->offset_t;
                    gradientAsLineDrawableUBO.width_t = patternUBO->width_t;
                    lineDrawableUBO = &gradientAsLineDrawableUBO;
                }
            } else if (lineSDFDrawable) {
                if (const auto* sdfUBO = getUBO<shaders::LineSDFDrawableUBO>(&getUniformBuffers(), shaders::idLineDrawableUBO)) {
                    gradientAsLineDrawableUBO.matrix = sdfUBO->matrix;
                    gradientAsLineDrawableUBO.ratio = sdfUBO->ratio;
                    gradientAsLineDrawableUBO.color_t = sdfUBO->color_t;
                    gradientAsLineDrawableUBO.blur_t = sdfUBO->blur_t;
                    gradientAsLineDrawableUBO.opacity_t = sdfUBO->opacity_t;
                    gradientAsLineDrawableUBO.gapwidth_t = sdfUBO->gapwidth_t;
                    gradientAsLineDrawableUBO.offset_t = sdfUBO->offset_t;
                    gradientAsLineDrawableUBO.width_t = sdfUBO->width_t;
                    lineFloorWidthT = sdfUBO->floorwidth_t;
                    lineDrawableUBO = &gradientAsLineDrawableUBO;
                }
            } else {
                lineDrawableUBO = getUBO<shaders::LineDrawableUBO>(&getUniformBuffers(), shaders::idLineDrawableUBO);
            }
        }
        if (lineDrawableUBO) {
            lineDrawable = true;
            matrix = lineDrawableUBO->matrix;
            colorT = lineDrawableUBO->color_t;
            lineBlurT = lineDrawableUBO->blur_t;
            opacityT = lineDrawableUBO->opacity_t;
            lineGapWidthT = lineDrawableUBO->gapwidth_t;
            lineOffsetT = lineDrawableUBO->offset_t;
            lineWidthT = lineDrawableUBO->width_t;
            lineRatio = lineDrawableUBO->ratio;
            if (isFillOutlineTriangulated) {
                color = SkColor4f{0.0f, 0.0f, 0.0f, 1.0f};
                if (const auto* props = getUBO<shaders::FillEvaluatedPropsUBO>(layerUniforms, shaders::idFillEvaluatedPropsUBO)) {
                    color = toSkColor(props->outline_color, props->opacity);
                }
                lineBlur = 0.0f;
                lineOffset = 0.0f;
                lineWidth = 1.0f;
                lineGapWidth = 0.0f;
                lineFloorWidth = 1.0f;
            } else if (const auto* props = getUBO<shaders::LineEvaluatedPropsUBO>(layerUniforms, shaders::idLineEvaluatedPropsUBO)) {
                color = (lineGradientDrawable || linePatternDrawable) ? SkColor4f{1.0f, 1.0f, 1.0f, props->opacity}
                                                                      : toSkColor(props->color, props->opacity);
                lineBlur = props->blur;
                lineOffset = props->offset * -1.0f;
                lineWidth = props->width;
                lineGapWidth = props->gapwidth;
                lineFloorWidth = props->floorwidth;
            }
        }
    } else if (getName().find("fill-outline-pattern") != std::string::npos) {
        const shaders::FillOutlinePatternDrawableUBO* patternDrawableUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        if (const auto* fillDrawableUnion = getUBO<shaders::FillDrawableUnionUBO>(layerUniforms, shaders::idFillDrawableUBO, getUBOIndex())) {
            patternDrawableUBO = &fillDrawableUnion->fillOutlinePatternDrawableUBO;
        }
#endif
        if (!patternDrawableUBO) {
            patternDrawableUBO = getUBO<shaders::FillOutlinePatternDrawableUBO>(&getUniformBuffers(), shaders::idFillDrawableUBO);
        }
        if (patternDrawableUBO) {
            fillOutlinePatternDrawable = true;
            matrix = patternDrawableUBO->matrix;
            fillPatternPixelCoordUpper = patternDrawableUBO->pixel_coord_upper;
            fillPatternPixelCoordLower = patternDrawableUBO->pixel_coord_lower;
            fillPatternTileRatio = patternDrawableUBO->tile_ratio;
            opacityT = patternDrawableUBO->opacity_t;
            if (const auto* props = getUBO<shaders::FillEvaluatedPropsUBO>(layerUniforms, shaders::idFillEvaluatedPropsUBO)) {
                color = SkColor4f{1.0f, 1.0f, 1.0f, props->opacity};
                fillPatternFade = props->fade;
                fillPatternFromScale = props->from_scale;
                fillPatternToScale = props->to_scale;
            }
            const shaders::FillOutlinePatternTilePropsUBO* tileProps = nullptr;
#if MLN_UBO_CONSOLIDATION
            if (const auto* fillTilePropsUnion = getUBO<shaders::FillTilePropsUnionUBO>(layerUniforms, shaders::idFillTilePropsUBO, getUBOIndex())) {
                tileProps = &fillTilePropsUnion->fillOutlinePatternTilePropsUBO;
            }
#endif
            if (!tileProps) {
                tileProps = getUBO<shaders::FillOutlinePatternTilePropsUBO>(&getUniformBuffers(), shaders::idFillTilePropsUBO);
            }
            if (tileProps) {
                fillPatternFrom = tileProps->pattern_from;
                fillPatternTo = tileProps->pattern_to;
                fillPatternTexsize = tileProps->texsize;
            }
        }
    } else if (getName().find("fill-pattern") != std::string::npos) {
        const shaders::FillPatternDrawableUBO* patternDrawableUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        if (const auto* fillDrawableUnion = getUBO<shaders::FillDrawableUnionUBO>(layerUniforms, shaders::idFillDrawableUBO, getUBOIndex())) {
            patternDrawableUBO = &fillDrawableUnion->fillPatternDrawableUBO;
        }
#endif
        if (!patternDrawableUBO) {
            patternDrawableUBO = getUBO<shaders::FillPatternDrawableUBO>(&getUniformBuffers(), shaders::idFillDrawableUBO);
        }
        if (patternDrawableUBO) {
            fillPatternDrawable = true;
            matrix = patternDrawableUBO->matrix;
            fillPatternPixelCoordUpper = patternDrawableUBO->pixel_coord_upper;
            fillPatternPixelCoordLower = patternDrawableUBO->pixel_coord_lower;
            fillPatternTileRatio = patternDrawableUBO->tile_ratio;
            opacityT = patternDrawableUBO->opacity_t;
            if (const auto* props = getUBO<shaders::FillEvaluatedPropsUBO>(layerUniforms, shaders::idFillEvaluatedPropsUBO)) {
                color = SkColor4f{1.0f, 1.0f, 1.0f, props->opacity};
                fillPatternFade = props->fade;
                fillPatternFromScale = props->from_scale;
                fillPatternToScale = props->to_scale;
            }
            const shaders::FillPatternTilePropsUBO* tileProps = nullptr;
#if MLN_UBO_CONSOLIDATION
            if (const auto* fillTilePropsUnion = getUBO<shaders::FillTilePropsUnionUBO>(layerUniforms, shaders::idFillTilePropsUBO, getUBOIndex())) {
                tileProps = &fillTilePropsUnion->fillPatternTilePropsUBO;
            }
#endif
            if (!tileProps) {
                tileProps = getUBO<shaders::FillPatternTilePropsUBO>(&getUniformBuffers(), shaders::idFillTilePropsUBO);
            }
            if (tileProps) {
                fillPatternFrom = tileProps->pattern_from;
                fillPatternTo = tileProps->pattern_to;
                fillPatternTexsize = tileProps->texsize;
            }
        }
    } else if (backgroundImageTexture) {
        const shaders::BackgroundPatternDrawableUBO* patternDrawableUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        if (const auto* backgroundDrawableUnion = getUBO<shaders::BackgroundDrawableUnionUBO>(layerUniforms, shaders::idBackgroundDrawableUBO, getUBOIndex())) {
            patternDrawableUBO = &backgroundDrawableUnion->backgroundPatternDrawableUBO;
        }
#endif
        if (!patternDrawableUBO) {
            patternDrawableUBO = getUBO<shaders::BackgroundPatternDrawableUBO>(&getUniformBuffers(), shaders::idBackgroundDrawableUBO);
        }
        if (patternDrawableUBO) {
            backgroundPatternDrawable = true;
            matrix = patternDrawableUBO->matrix;
            backgroundPatternPixelCoordUpper = patternDrawableUBO->pixel_coord_upper;
            backgroundPatternPixelCoordLower = patternDrawableUBO->pixel_coord_lower;
            backgroundPatternTileUnitsToPixels = patternDrawableUBO->tile_units_to_pixels;
            if (const auto* props = getUBO<shaders::BackgroundPatternPropsUBO>(layerUniforms, shaders::idBackgroundPropsUBO)) {
                backgroundPatternTlA = props->pattern_tl_a;
                backgroundPatternBrA = props->pattern_br_a;
                backgroundPatternTlB = props->pattern_tl_b;
                backgroundPatternBrB = props->pattern_br_b;
                backgroundPatternSizeA = props->pattern_size_a;
                backgroundPatternSizeB = props->pattern_size_b;
                backgroundPatternScaleA = props->scale_a;
                backgroundPatternScaleB = props->scale_b;
                backgroundPatternMix = props->mix;
                backgroundPatternOpacity = props->opacity;
            }
        }
    } else if (getName().find("fill-outline") != std::string::npos && getName().find("pattern") == std::string::npos) {
        const shaders::FillOutlineDrawableUBO* outlineDrawableUBO = nullptr;
#if MLN_UBO_CONSOLIDATION
        if (const auto* fillDrawableUnion = getUBO<shaders::FillDrawableUnionUBO>(layerUniforms, shaders::idFillDrawableUBO, getUBOIndex())) {
            outlineDrawableUBO = &fillDrawableUnion->fillOutlineDrawableUBO;
        }
#endif
        if (!outlineDrawableUBO) {
            outlineDrawableUBO = getUBO<shaders::FillOutlineDrawableUBO>(&getUniformBuffers(), shaders::idFillDrawableUBO);
        }
        if (outlineDrawableUBO) {
            fillOutlineDrawable = true;
            matrix = outlineDrawableUBO->matrix;
            colorT = outlineDrawableUBO->outline_color_t;
            opacityT = outlineDrawableUBO->opacity_t;
            if (const auto* props = getUBO<shaders::FillEvaluatedPropsUBO>(layerUniforms, shaders::idFillEvaluatedPropsUBO)) {
                color = toRawSkColor(props->outline_color);
                color.fA *= props->opacity;
            }
        }
    } else if (const auto* drawableUBO = getUBO<shaders::FillDrawableUBO>(&getUniformBuffers(), shaders::idFillDrawableUBO)) {
        matrix = drawableUBO->matrix;
        colorT = drawableUBO->color_t;
        opacityT = drawableUBO->opacity_t;
        if (const auto* props = getUBO<shaders::FillEvaluatedPropsUBO>(layerUniforms, shaders::idFillEvaluatedPropsUBO)) {
            color = toSkColor(props->color, props->opacity);
        }
    } else if (const auto* backgroundDrawableUBO = getUBO<shaders::BackgroundDrawableUBO>(&getUniformBuffers(),
                                                                                           shaders::idBackgroundDrawableUBO)) {
        matrix = backgroundDrawableUBO->matrix;
        if (const auto* props = getUBO<shaders::BackgroundPropsUBO>(layerUniforms, shaders::idBackgroundPropsUBO)) {
            color = toSkColor(props->color, props->opacity);
        }
    }

    VertexReader vertexReader;
    if (!vertices.empty() && vertexDataType == gfx::AttributeDataType::Short2) {
        vertexReader = VertexReader{vertices.data(), vertexCount, sizeof(std::int16_t) * 2};
    } else if (const auto& attrs = getVertexAttributes()) {
        const std::size_t fallbackPositionAttributeId = (heatmapDrawable || heatmapTextureDrawable) ? static_cast<std::size_t>(shaders::idHeatmapPosVertexAttribute)
                                                        : ((hillshadePrepareDrawable || hillshadeDrawable) ? static_cast<std::size_t>(shaders::idHillshadePosVertexAttribute)
                                                        : (colorReliefDrawable ? static_cast<std::size_t>(shaders::idColorReliefPosVertexAttribute)
                                                          : (fillExtrusionDrawable ? static_cast<std::size_t>(shaders::idFillExtrusionPosVertexAttribute)
                                                          : ((collisionBoxDrawable || collisionCircleDrawable) ? static_cast<std::size_t>(shaders::idCollisionPosVertexAttribute)
                                                        : ((symbolIconDrawable || symbolSDFDrawable || symbolTextAndIconDrawable) ? static_cast<std::size_t>(shaders::idSymbolPosOffsetVertexAttribute)
                                                          : (rasterDrawable ? static_cast<std::size_t>(shaders::idRasterPosVertexAttribute) : (circleDrawable ? static_cast<std::size_t>(shaders::idCirclePosVertexAttribute) : (lineDrawable
                                                                                                                                                             ? static_cast<std::size_t>(shaders::idLinePosNormalVertexAttribute)
                                                                                                                                                             : static_cast<std::size_t>(shaders::idFillPosVertexAttribute)))))))));
        const auto& attr = attrs->get(positionAttributeId) ? attrs->get(positionAttributeId)
                                                           : attrs->get(fallbackPositionAttributeId);
        if (attr && attr->getSharedRawData() && (attr->getSharedType() == gfx::AttributeDataType::Short2 ||
                                                attr->getSharedType() == gfx::AttributeDataType::Short4)) {
            const auto* raw = static_cast<const std::uint8_t*>(attr->getSharedRawData()->getRawData());
            vertexReader = VertexReader{raw + attr->getSharedOffset() + attr->getSharedVertexOffset() * attr->getSharedStride(),
                                        vertexCount ? vertexCount : attr->getSharedRawData()->getRawCount(),
                                        attr->getSharedStride()};
        } else if (attr && (attr->getDataType() == gfx::AttributeDataType::Short2 ||
                            attr->getDataType() == gfx::AttributeDataType::Short4) && !attr->getRawData().empty()) {
            const auto stride = attr->getDataType() == gfx::AttributeDataType::Short4 ? sizeof(std::int16_t) * 4
                                                                                      : sizeof(std::int16_t) * 2;
            vertexReader = VertexReader{attr->getRawData().data(), vertexCount, stride};
        }
    }
    if (!vertexReader.data) {
        return;
    }

    const auto specification = [&] {
        if (symbolIconDrawable) return symbolIconMeshSpecification();
        if (symbolTextAndIconDrawable) return symbolTextAndIconMeshSpecification();
        if (symbolSDFDrawable) return symbolSDFMeshSpecification();
        if (collisionCircleDrawable) return collisionCircleMeshSpecification();
        if (heatmapTextureDrawable) return heatmapTextureMeshSpecification();
        if (heatmapDrawable) return heatmapMeshSpecification();
        if (hillshadePrepareDrawable) return hillshadePrepareMeshSpecification();
        if (hillshadeDrawable) return hillshadeMeshSpecification();
        if (colorReliefDrawable) return colorReliefMeshSpecification();
        if (fillExtrusionPatternDrawable) return fillExtrusionPatternMeshSpecification();
        if (fillExtrusionDrawable) return fillExtrusionMeshSpecification();
        if (lineGradientDrawable) return lineGradientMeshSpecification();
        if (linePatternDrawable) return linePatternMeshSpecification();
        if (lineSDFDrawable) return lineSDFMeshSpecification();
        if (rasterDrawable) return rasterMeshSpecification();
        if (backgroundPatternDrawable) return backgroundPatternMeshSpecification();
        if (fillPatternDrawable) return fillPatternMeshSpecification();
        if (circleDrawable) return circleMeshSpecification();
        if (lineDrawable) return lineMeshSpecification();
        return solidColorMeshSpecification();
    }();
    if (!specification) {
        return;
    }

    if (vertexReader.count > std::numeric_limits<std::uint16_t>::max()) {
        return;
    }

    UByte4Reader lineDataReader;
    Short4Reader fillExtrusionNormalReader;
    Float2Reader lineWidthReader;
    Float2Reader lineGapWidthReader;
    Float2Reader lineBlurReader;
    Float2Reader lineOffsetReader;
    Float2Reader lineFloorWidthReader;
    Float2Reader fillExtrusionBaseReader;
    Float2Reader fillExtrusionHeightReader;
    Float4Reader circleColorReader;
    Float2Reader circleRadiusReader;
    Float2Reader circleBlurReader;
    Float2Reader circleOpacityReader;
    Float4Reader circleStrokeColorReader;
    Float2Reader circleStrokeWidthReader;
    Float2Reader circleStrokeOpacityReader;
    Float2Reader heatmapWeightReader;
    Float2Reader heatmapRadiusReader;
    Short4Reader symbolPosOffsetReader;
    UShort4Reader symbolDataReader;
    Short4Reader symbolPixelOffsetReader;
    Float3Reader symbolProjectedPosReader;
    FloatReader symbolFadeOpacityReader;
    Float2Reader symbolOpacityReader;
    Float4Reader symbolFillColorReader;
    Float4Reader symbolHaloColorReader;
    Float2Reader symbolHaloWidthReader;
    Float2Reader symbolHaloBlurReader;
    VertexReader collisionAnchorPosReader;
    VertexReader collisionExtrudeReader;
    UShort2Reader collisionPlacedReader;
    Float2Reader collisionShiftReader;
    VertexReader rasterTexturePosReader;
    UShort4Reader fillPatternFromReader;
    UShort4Reader fillPatternToReader;
    if (collisionBoxDrawable || collisionCircleDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            const auto initShort2Reader = [](const gfx::VertexAttribute& attr, VertexReader& reader) {
                if (attr.getSharedRawData() && attr.getSharedType() == gfx::AttributeDataType::Short2) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if (attr.getDataType() == gfx::AttributeDataType::Short2 && !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.stride = sizeof(std::int16_t) * 2;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            const auto initUShort2Reader = [](const gfx::VertexAttribute& attr, UShort2Reader& reader) {
                if (attr.getSharedRawData() && attr.getSharedType() == gfx::AttributeDataType::UShort2) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if (attr.getDataType() == gfx::AttributeDataType::UShort2 && !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.stride = sizeof(std::uint16_t) * 2;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            const auto initFloat2Reader = [](const gfx::VertexAttribute& attr, Float2Reader& reader) {
                reader.attribute = &attr;
                if (attr.getSharedRawData() && attr.getSharedType() == gfx::AttributeDataType::Float2) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if (attr.getDataType() == gfx::AttributeDataType::Float2 && !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.stride = sizeof(float) * 2;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            if (const auto& attr = attrs->get(shaders::idCollisionAnchorPosVertexAttribute)) initShort2Reader(*attr, collisionAnchorPosReader);
            if (const auto& attr = attrs->get(shaders::idCollisionExtrudeVertexAttribute)) initShort2Reader(*attr, collisionExtrudeReader);
            if (const auto& attr = attrs->get(shaders::idCollisionPlacedVertexAttribute)) initUShort2Reader(*attr, collisionPlacedReader);
            if (const auto& attr = attrs->get(shaders::idCollisionShiftVertexAttribute)) initFloat2Reader(*attr, collisionShiftReader);
        }
        if (!collisionAnchorPosReader.data || !collisionExtrudeReader.data || !collisionPlacedReader.data) {
            return;
        }
    } else if (symbolIconDrawable || symbolSDFDrawable || symbolTextAndIconDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            const auto initShort4Reader = [](const gfx::VertexAttribute& attr, Short4Reader& reader) {
                if (attr.getSharedRawData() && attr.getSharedType() == gfx::AttributeDataType::Short4) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if (attr.getDataType() == gfx::AttributeDataType::Short4 && !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.stride = sizeof(std::int16_t) * 4;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            const auto initUShort4Reader = [](const gfx::VertexAttribute& attr, UShort4Reader& reader) {
                if (attr.getSharedRawData() && attr.getSharedType() == gfx::AttributeDataType::UShort4) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if (attr.getDataType() == gfx::AttributeDataType::UShort4 && !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.stride = sizeof(std::uint16_t) * 4;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            const auto initFloatReader = [](const gfx::VertexAttribute& attr, FloatReader& reader) {
                reader.attribute = &attr;
                if (attr.getSharedRawData() && attr.getSharedType() == gfx::AttributeDataType::Float) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if (attr.getDataType() == gfx::AttributeDataType::Float && !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.stride = sizeof(float);
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            const auto initFloat2Reader = [](const gfx::VertexAttribute& attr, Float2Reader& reader) {
                reader.attribute = &attr;
                if (attr.getSharedRawData() && (attr.getSharedType() == gfx::AttributeDataType::Float2 ||
                                                attr.getSharedType() == gfx::AttributeDataType::Float)) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.components = attr.getSharedType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if ((attr.getDataType() == gfx::AttributeDataType::Float2 || attr.getDataType() == gfx::AttributeDataType::Float) &&
                           !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.components = attr.getDataType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.stride = sizeof(float) * reader.components;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            const auto initFloat3Reader = [](const gfx::VertexAttribute& attr, Float3Reader& reader) {
                reader.attribute = &attr;
                if (attr.getSharedRawData() && attr.getSharedType() == gfx::AttributeDataType::Float3) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if (attr.getDataType() == gfx::AttributeDataType::Float3 && !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.stride = sizeof(float) * 3;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };

            if (const auto& attr = attrs->get(shaders::idSymbolPosOffsetVertexAttribute)) initShort4Reader(*attr, symbolPosOffsetReader);
            if (const auto& attr = attrs->get(shaders::idSymbolDataVertexAttribute)) initUShort4Reader(*attr, symbolDataReader);
            if (!symbolTextAndIconDrawable) {
                if (const auto& attr = attrs->get(shaders::idSymbolPixelOffsetVertexAttribute)) initShort4Reader(*attr, symbolPixelOffsetReader);
            }
            if (const auto& attr = attrs->get(shaders::idSymbolProjectedPosVertexAttribute)) initFloat3Reader(*attr, symbolProjectedPosReader);
            if (const auto& attr = attrs->get(shaders::idSymbolFadeOpacityVertexAttribute)) initFloatReader(*attr, symbolFadeOpacityReader);
            if (const auto& attr = attrs->get(shaders::idSymbolOpacityVertexAttribute)) initFloat2Reader(*attr, symbolOpacityReader);
            if (symbolSDFDrawable || symbolTextAndIconDrawable) {
                const auto initFloat4Reader = [](const gfx::VertexAttribute& attr, Float4Reader& reader) {
                    reader.attribute = &attr;
                    if (attr.getSharedRawData() && (attr.getSharedType() == gfx::AttributeDataType::Float4 ||
                                                    attr.getSharedType() == gfx::AttributeDataType::Float2)) {
                        const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                        reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                        reader.stride = attr.getSharedStride();
                        reader.components = attr.getSharedType() == gfx::AttributeDataType::Float2 ? 2 : 4;
                        reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                    } else if ((attr.getDataType() == gfx::AttributeDataType::Float4 || attr.getDataType() == gfx::AttributeDataType::Float2) &&
                               !attr.getRawData().empty()) {
                        reader.data = attr.getRawData().data();
                        reader.components = attr.getDataType() == gfx::AttributeDataType::Float2 ? 2 : 4;
                        reader.stride = sizeof(float) * reader.components;
                        reader.count = attr.getRawData().size() / reader.stride;
                    }
                };
                if (const auto& attr = attrs->get(shaders::idSymbolColorVertexAttribute)) initFloat4Reader(*attr, symbolFillColorReader);
                if (const auto& attr = attrs->get(shaders::idSymbolHaloColorVertexAttribute)) initFloat4Reader(*attr, symbolHaloColorReader);
                if (const auto& attr = attrs->get(shaders::idSymbolHaloWidthVertexAttribute)) initFloat2Reader(*attr, symbolHaloWidthReader);
                if (const auto& attr = attrs->get(shaders::idSymbolHaloBlurVertexAttribute)) initFloat2Reader(*attr, symbolHaloBlurReader);
            }
        }
        if (!symbolPosOffsetReader.data || !symbolDataReader.data || (!symbolTextAndIconDrawable && !symbolPixelOffsetReader.data) ||
            !symbolProjectedPosReader.data || !symbolFadeOpacityReader.data) {
            return;
        }
    } else if (heatmapDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            const auto initFloat2Reader = [](const gfx::VertexAttribute& attr, Float2Reader& reader) {
                reader.attribute = &attr;
                if (attr.getSharedRawData() && (attr.getSharedType() == gfx::AttributeDataType::Float2 ||
                                                attr.getSharedType() == gfx::AttributeDataType::Float)) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.components = attr.getSharedType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if ((attr.getDataType() == gfx::AttributeDataType::Float2 || attr.getDataType() == gfx::AttributeDataType::Float) &&
                           !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.components = attr.getDataType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.stride = sizeof(float) * reader.components;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            if (const auto& attr = attrs->get(shaders::idHeatmapWeightVertexAttribute)) initFloat2Reader(*attr, heatmapWeightReader);
            if (const auto& attr = attrs->get(shaders::idHeatmapRadiusVertexAttribute)) initFloat2Reader(*attr, heatmapRadiusReader);
        }
    } else if (fillExtrusionDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            if (const auto& attr = attrs->get(shaders::idFillExtrusionNormalEdVertexAttribute)) {
                if (attr->getSharedRawData() && attr->getSharedType() == gfx::AttributeDataType::Short4) {
                    const auto offset = attr->getSharedOffset() + attr->getSharedVertexOffset() * attr->getSharedStride();
                    fillExtrusionNormalReader.data = static_cast<const std::uint8_t*>(attr->getSharedRawData()->getRawData()) + offset;
                    fillExtrusionNormalReader.stride = attr->getSharedStride();
                    fillExtrusionNormalReader.count = attr->getSharedRawData()->getRawCount() - attr->getSharedVertexOffset();
                } else if (attr->getDataType() == gfx::AttributeDataType::Short4 && !attr->getRawData().empty()) {
                    fillExtrusionNormalReader.data = attr->getRawData().data();
                    fillExtrusionNormalReader.stride = sizeof(std::int16_t) * 4;
                    fillExtrusionNormalReader.count = attr->getRawData().size() / fillExtrusionNormalReader.stride;
                }
            }

            const auto initFloat2Reader = [](const gfx::VertexAttribute& attr, Float2Reader& reader) {
                reader.attribute = &attr;
                if (attr.getSharedRawData() && (attr.getSharedType() == gfx::AttributeDataType::Float2 ||
                                                attr.getSharedType() == gfx::AttributeDataType::Float)) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.components = attr.getSharedType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if ((attr.getDataType() == gfx::AttributeDataType::Float2 ||
                            attr.getDataType() == gfx::AttributeDataType::Float) &&
                           !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.components = attr.getDataType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.stride = sizeof(float) * reader.components;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            if (const auto& attr = attrs->get(shaders::idFillExtrusionBaseVertexAttribute)) initFloat2Reader(*attr, fillExtrusionBaseReader);
            if (const auto& attr = attrs->get(shaders::idFillExtrusionHeightVertexAttribute)) initFloat2Reader(*attr, fillExtrusionHeightReader);

            const auto initUShort4Reader = [](const gfx::VertexAttribute& attr, UShort4Reader& reader) {
                if (attr.getSharedRawData() && attr.getSharedType() == gfx::AttributeDataType::UShort4) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if (attr.getDataType() == gfx::AttributeDataType::UShort4 && !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.stride = sizeof(std::uint16_t) * 4;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            if (const auto& attr = attrs->get(shaders::idFillExtrusionPatternFromVertexAttribute)) initUShort4Reader(*attr, fillPatternFromReader);
            if (const auto& attr = attrs->get(shaders::idFillExtrusionPatternToVertexAttribute)) initUShort4Reader(*attr, fillPatternToReader);
        }
        if (!fillExtrusionNormalReader.data) {
            return;
        }
    } else if (lineDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            if (const auto& dataAttr = attrs->get(shaders::idLineDataVertexAttribute)) {
                if (dataAttr->getSharedRawData() && dataAttr->getSharedType() == gfx::AttributeDataType::UByte4) {
                    const auto offset = dataAttr->getSharedOffset() + dataAttr->getSharedVertexOffset() * dataAttr->getSharedStride();
                    lineDataReader.data = static_cast<const std::uint8_t*>(dataAttr->getSharedRawData()->getRawData()) + offset;
                    lineDataReader.stride = dataAttr->getSharedStride();
                    lineDataReader.count = dataAttr->getSharedRawData()->getRawCount() - dataAttr->getSharedVertexOffset();
                } else if (dataAttr->getDataType() == gfx::AttributeDataType::UByte4 && !dataAttr->getRawData().empty()) {
                    lineDataReader.data = dataAttr->getRawData().data();
                    lineDataReader.stride = sizeof(std::uint8_t) * 4;
                    lineDataReader.count = dataAttr->getRawData().size() / lineDataReader.stride;
                }
            }
            const auto initFloat2Reader = [](const gfx::VertexAttribute& attr, Float2Reader& reader) {
                reader.attribute = &attr;
                if (attr.getSharedRawData() && (attr.getSharedType() == gfx::AttributeDataType::Float2 ||
                                                attr.getSharedType() == gfx::AttributeDataType::Float)) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.components = attr.getSharedType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if ((attr.getDataType() == gfx::AttributeDataType::Float2 ||
                            attr.getDataType() == gfx::AttributeDataType::Float) &&
                           !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.components = attr.getDataType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.stride = sizeof(float) * reader.components;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            if (const auto& widthAttr = attrs->get(shaders::idLineWidthVertexAttribute)) {
                initFloat2Reader(*widthAttr, lineWidthReader);
            }
            if (const auto& gapWidthAttr = attrs->get(shaders::idLineGapWidthVertexAttribute)) {
                initFloat2Reader(*gapWidthAttr, lineGapWidthReader);
            }
            if (const auto& blurAttr = attrs->get(shaders::idLineBlurVertexAttribute)) {
                initFloat2Reader(*blurAttr, lineBlurReader);
            }
            if (const auto& offsetAttr = attrs->get(shaders::idLineOffsetVertexAttribute)) {
                initFloat2Reader(*offsetAttr, lineOffsetReader);
            }
            if (const auto& floorWidthAttr = attrs->get(shaders::idLineFloorWidthVertexAttribute)) {
                initFloat2Reader(*floorWidthAttr, lineFloorWidthReader);
            }
        }
        if (!lineDataReader.data) {
            return;
        }
    } else if (circleDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            const auto initFloat2Reader = [](const gfx::VertexAttribute& attr, Float2Reader& reader) {
                reader.attribute = &attr;
                if (attr.getSharedRawData() && (attr.getSharedType() == gfx::AttributeDataType::Float2 ||
                                                attr.getSharedType() == gfx::AttributeDataType::Float)) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.components = attr.getSharedType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if ((attr.getDataType() == gfx::AttributeDataType::Float2 || attr.getDataType() == gfx::AttributeDataType::Float) &&
                           !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.components = attr.getDataType() == gfx::AttributeDataType::Float ? 1 : 2;
                    reader.stride = sizeof(float) * reader.components;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            const auto initFloat4Reader = [](const gfx::VertexAttribute& attr, Float4Reader& reader) {
                reader.attribute = &attr;
                if (attr.getSharedRawData() && (attr.getSharedType() == gfx::AttributeDataType::Float4 ||
                                                attr.getSharedType() == gfx::AttributeDataType::Float2)) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.components = attr.getSharedType() == gfx::AttributeDataType::Float2 ? 2 : 4;
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if ((attr.getDataType() == gfx::AttributeDataType::Float4 || attr.getDataType() == gfx::AttributeDataType::Float2) &&
                           !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.components = attr.getDataType() == gfx::AttributeDataType::Float2 ? 2 : 4;
                    reader.stride = sizeof(float) * reader.components;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            if (const auto& attr = attrs->get(shaders::idCircleColorVertexAttribute)) initFloat4Reader(*attr, circleColorReader);
            if (const auto& attr = attrs->get(shaders::idCircleRadiusVertexAttribute)) initFloat2Reader(*attr, circleRadiusReader);
            if (const auto& attr = attrs->get(shaders::idCircleBlurVertexAttribute)) initFloat2Reader(*attr, circleBlurReader);
            if (const auto& attr = attrs->get(shaders::idCircleOpacityVertexAttribute)) initFloat2Reader(*attr, circleOpacityReader);
            if (const auto& attr = attrs->get(shaders::idCircleStrokeColorVertexAttribute)) initFloat4Reader(*attr, circleStrokeColorReader);
            if (const auto& attr = attrs->get(shaders::idCircleStrokeWidthVertexAttribute)) initFloat2Reader(*attr, circleStrokeWidthReader);
            if (const auto& attr = attrs->get(shaders::idCircleStrokeOpacityVertexAttribute)) initFloat2Reader(*attr, circleStrokeOpacityReader);
        }
    } else if (hillshadePrepareDrawable || hillshadeDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            if (const auto& attr = attrs->get(shaders::idHillshadeTexturePosVertexAttribute)) {
                if (attr->getSharedRawData() && attr->getSharedType() == gfx::AttributeDataType::Short2) {
                    const auto offset = attr->getSharedOffset() + attr->getSharedVertexOffset() * attr->getSharedStride();
                    rasterTexturePosReader = VertexReader{static_cast<const std::uint8_t*>(attr->getSharedRawData()->getRawData()) + offset,
                                                          attr->getSharedRawData()->getRawCount() - attr->getSharedVertexOffset(),
                                                          attr->getSharedStride()};
                } else if (attr->getDataType() == gfx::AttributeDataType::Short2 && !attr->getRawData().empty()) {
                    rasterTexturePosReader = VertexReader{attr->getRawData().data(), attr->getRawData().size() / (sizeof(std::int16_t) * 2), sizeof(std::int16_t) * 2};
                }
            }
        }
        if (!rasterTexturePosReader.data) {
            return;
        }
    } else if (colorReliefDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            if (const auto& attr = attrs->get(shaders::idColorReliefTexturePosVertexAttribute)) {
                if (attr->getSharedRawData() && attr->getSharedType() == gfx::AttributeDataType::Short2) {
                    const auto offset = attr->getSharedOffset() + attr->getSharedVertexOffset() * attr->getSharedStride();
                    rasterTexturePosReader = VertexReader{static_cast<const std::uint8_t*>(attr->getSharedRawData()->getRawData()) + offset,
                                                          attr->getSharedRawData()->getRawCount() - attr->getSharedVertexOffset(),
                                                          attr->getSharedStride()};
                } else if (attr->getDataType() == gfx::AttributeDataType::Short2 && !attr->getRawData().empty()) {
                    rasterTexturePosReader = VertexReader{attr->getRawData().data(), attr->getRawData().size() / (sizeof(std::int16_t) * 2), sizeof(std::int16_t) * 2};
                }
            }
        }
        if (!rasterTexturePosReader.data) {
            return;
        }
    } else if (rasterDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            if (const auto& attr = attrs->get(shaders::idRasterTexturePosVertexAttribute)) {
                if (attr->getSharedRawData() && attr->getSharedType() == gfx::AttributeDataType::Short2) {
                    const auto offset = attr->getSharedOffset() + attr->getSharedVertexOffset() * attr->getSharedStride();
                    rasterTexturePosReader = VertexReader{static_cast<const std::uint8_t*>(attr->getSharedRawData()->getRawData()) + offset,
                                                          attr->getSharedRawData()->getRawCount() - attr->getSharedVertexOffset(),
                                                          attr->getSharedStride()};
                } else if (attr->getDataType() == gfx::AttributeDataType::Short2 && !attr->getRawData().empty()) {
                    rasterTexturePosReader = VertexReader{attr->getRawData().data(), attr->getRawData().size() / (sizeof(std::int16_t) * 2), sizeof(std::int16_t) * 2};
                }
            }
        }
        if (!rasterTexturePosReader.data) {
            return;
        }
    } else if (fillPatternDrawable || fillOutlinePatternDrawable) {
        if (const auto& attrs = getVertexAttributes()) {
            const auto initUShort4Reader = [](const gfx::VertexAttribute& attr, UShort4Reader& reader) {
                if (attr.getSharedRawData() && attr.getSharedType() == gfx::AttributeDataType::UShort4) {
                    const auto offset = attr.getSharedOffset() + attr.getSharedVertexOffset() * attr.getSharedStride();
                    reader.data = static_cast<const std::uint8_t*>(attr.getSharedRawData()->getRawData()) + offset;
                    reader.stride = attr.getSharedStride();
                    reader.count = attr.getSharedRawData()->getRawCount() - attr.getSharedVertexOffset();
                } else if (attr.getDataType() == gfx::AttributeDataType::UShort4 && !attr.getRawData().empty()) {
                    reader.data = attr.getRawData().data();
                    reader.stride = sizeof(std::uint16_t) * 4;
                    reader.count = attr.getRawData().size() / reader.stride;
                }
            };
            if (const auto& attr = attrs->get(shaders::idFillPatternFromVertexAttribute)) initUShort4Reader(*attr, fillPatternFromReader);
            if (const auto& attr = attrs->get(shaders::idFillPatternToVertexAttribute)) initUShort4Reader(*attr, fillPatternToReader);
        }
    }

    std::vector<MeshVertex> meshVertices(vertexReader.count);
    for (std::size_t i = 0; i < vertexReader.count; ++i) {
        if (!vertexReader.read(static_cast<std::uint16_t>(i), meshVertices[i].position[0], meshVertices[i].position[1])) {
            return;
        }
        if (collisionBoxDrawable || collisionCircleDrawable) {
            float anchorX = 0.0f;
            float anchorY = 0.0f;
            float extrudeX = 0.0f;
            float extrudeY = 0.0f;
            std::array<float, 2> placed;
            std::array<float, 2> shift = {0.0f, 0.0f};
            if (!collisionAnchorPosReader.read(static_cast<std::uint16_t>(i), anchorX, anchorY) ||
                !collisionExtrudeReader.read(static_cast<std::uint16_t>(i), extrudeX, extrudeY) ||
                !collisionPlacedReader.read(static_cast<std::uint16_t>(i), placed)) {
                return;
            }
            collisionShiftReader.read(static_cast<std::uint16_t>(i), shift);
            meshVertices[i].collisionAnchorPos[0] = anchorX;
            meshVertices[i].collisionAnchorPos[1] = anchorY;
            meshVertices[i].collisionExtrude[0] = extrudeX;
            meshVertices[i].collisionExtrude[1] = extrudeY;
            meshVertices[i].collisionPlaced[0] = placed[0];
            meshVertices[i].collisionPlaced[1] = placed[1];
            meshVertices[i].collisionShift[0] = shift[0];
            meshVertices[i].collisionShift[1] = shift[1];
        } else if (symbolIconDrawable || symbolSDFDrawable || symbolTextAndIconDrawable) {
            std::array<std::int16_t, 4> posOffset;
            std::array<float, 4> data;
            std::array<std::int16_t, 4> pixelOffset{};
            std::array<float, 3> projectedPos;
            float fadeOpacity = 0.0f;
            if (!symbolPosOffsetReader.read(static_cast<std::uint16_t>(i), posOffset) ||
                !symbolDataReader.read(static_cast<std::uint16_t>(i), data) ||
                (!symbolTextAndIconDrawable && !symbolPixelOffsetReader.read(static_cast<std::uint16_t>(i), pixelOffset)) ||
                !symbolProjectedPosReader.read(static_cast<std::uint16_t>(i), projectedPos) ||
                !symbolFadeOpacityReader.read(static_cast<std::uint16_t>(i), fadeOpacity)) {
                return;
            }
            std::array<float, 2> opacity = {symbolOpacity, symbolOpacity};
            symbolOpacityReader.read(static_cast<std::uint16_t>(i), opacity);
            for (std::size_t j = 0; j < 4; ++j) {
                meshVertices[i].symbolPosOffset[j] = static_cast<float>(posOffset[j]);
                meshVertices[i].symbolData[j] = data[j];
                meshVertices[i].symbolPixelOffset[j] = static_cast<float>(pixelOffset[j]);
            }
            for (std::size_t j = 0; j < 3; ++j) {
                meshVertices[i].symbolProjectedPos[j] = projectedPos[j];
            }
            meshVertices[i].symbolFadeOpacity = fadeOpacity;
            meshVertices[i].symbolOpacity[0] = opacity[0];
            meshVertices[i].symbolOpacity[1] = opacity[1];
            if (symbolSDFDrawable || symbolTextAndIconDrawable) {
                auto fillColor = symbolFillColor;
                auto haloColor = symbolHaloColor;
                auto haloWidth = symbolHaloWidth;
                auto haloBlur = symbolHaloBlur;
                const auto vertexOpacity = unpackMixFloat(opacity, symbolOpacityT);
                std::array<float, 4> packedFillColor;
                if (symbolFillColorReader.read(static_cast<std::uint16_t>(i), packedFillColor)) fillColor = unpackMixColor(packedFillColor, symbolFillColorT);
                std::array<float, 4> packedHaloColor;
                if (symbolHaloColorReader.read(static_cast<std::uint16_t>(i), packedHaloColor)) haloColor = unpackMixColor(packedHaloColor, symbolHaloColorT);
                std::array<float, 2> packedHaloWidth;
                if (symbolHaloWidthReader.read(static_cast<std::uint16_t>(i), packedHaloWidth)) haloWidth = unpackMixFloat(packedHaloWidth, symbolHaloWidthT);
                std::array<float, 2> packedHaloBlur;
                if (symbolHaloBlurReader.read(static_cast<std::uint16_t>(i), packedHaloBlur)) haloBlur = unpackMixFloat(packedHaloBlur, symbolHaloBlurT);
                meshVertices[i].symbolFillColor[0] = fillColor.fR;
                meshVertices[i].symbolFillColor[1] = fillColor.fG;
                meshVertices[i].symbolFillColor[2] = fillColor.fB;
                meshVertices[i].symbolFillColor[3] = fillColor.fA;
                meshVertices[i].symbolHaloColor[0] = haloColor.fR;
                meshVertices[i].symbolHaloColor[1] = haloColor.fG;
                meshVertices[i].symbolHaloColor[2] = haloColor.fB;
                meshVertices[i].symbolHaloColor[3] = haloColor.fA;
                meshVertices[i].symbolHaloWidth[0] = vertexOpacity;
                meshVertices[i].symbolHaloWidth[1] = haloWidth;
                meshVertices[i].symbolHaloBlur[0] = haloBlur;
                meshVertices[i].symbolHaloBlur[1] = haloBlur;
            }
        } else if (rasterDrawable || colorReliefDrawable || hillshadePrepareDrawable || hillshadeDrawable) {
            if (!rasterTexturePosReader.read(static_cast<std::uint16_t>(i), meshVertices[i].rasterTexturePos[0], meshVertices[i].rasterTexturePos[1])) {
                return;
            }
        } else if (heatmapDrawable) {
            std::array<float, 2> weight = {heatmapWeight, heatmapWeight};
            std::array<float, 2> radius = {heatmapRadius, heatmapRadius};
            heatmapWeightReader.read(static_cast<std::uint16_t>(i), weight);
            heatmapRadiusReader.read(static_cast<std::uint16_t>(i), radius);
            meshVertices[i].heatmapWeight[0] = weight[0];
            meshVertices[i].heatmapWeight[1] = weight[1];
            meshVertices[i].heatmapRadius[0] = radius[0];
            meshVertices[i].heatmapRadius[1] = radius[1];
        } else if (fillExtrusionDrawable) {
            std::array<std::int16_t, 4> normalEd;
            if (!fillExtrusionNormalReader.read(static_cast<std::uint16_t>(i), normalEd)) {
                return;
            }
            auto vertexBase = fillExtrusionBase;
            auto vertexHeight = fillExtrusionHeight;
            std::array<float, 2> packedBase;
            if (fillExtrusionBaseReader.read(static_cast<std::uint16_t>(i), packedBase)) {
                vertexBase = unpackMixFloat(packedBase, fillExtrusionBaseT);
            }
            std::array<float, 2> packedHeight;
            if (fillExtrusionHeightReader.read(static_cast<std::uint16_t>(i), packedHeight)) {
                vertexHeight = unpackMixFloat(packedHeight, fillExtrusionHeightT);
            }
            const auto isTopVertex = std::abs(normalEd[0] % 2) != 0;
            meshVertices[i].fillExtrusionZ = std::max(isTopVertex ? vertexHeight : vertexBase, 0.0f);
            meshVertices[i].fillExtrusionNormal[0] = static_cast<float>(normalEd[0]);
            meshVertices[i].fillExtrusionNormal[1] = static_cast<float>(normalEd[1]);
            meshVertices[i].fillExtrusionNormal[2] = static_cast<float>(normalEd[2]);
            meshVertices[i].fillExtrusionT = isTopVertex ? 1.0f : 0.0f;
            if (fillExtrusionPatternDrawable) {
                auto vertexPatternFrom = fillPatternFrom;
                auto vertexPatternTo = fillPatternTo;
                fillPatternFromReader.read(static_cast<std::uint16_t>(i), vertexPatternFrom);
                fillPatternToReader.read(static_cast<std::uint16_t>(i), vertexPatternTo);
                std::copy(vertexPatternFrom.begin(), vertexPatternFrom.end(), meshVertices[i].fillPatternFrom);
                std::copy(vertexPatternTo.begin(), vertexPatternTo.end(), meshVertices[i].fillPatternTo);

                const auto patternPos = isTopVertex ? std::array<float, 2>{meshVertices[i].position[0], meshVertices[i].position[1]}
                                                    : std::array<float, 2>{static_cast<float>(normalEd[3]), meshVertices[i].fillExtrusionZ * fillExtrusionHeightFactor};
                const std::array<float, 2> displaySizeA = {(vertexPatternFrom[2] - vertexPatternFrom[0]) / parameters.pixelRatio,
                                                           (vertexPatternFrom[3] - vertexPatternFrom[1]) / parameters.pixelRatio};
                const std::array<float, 2> displaySizeB = {(vertexPatternTo[2] - vertexPatternTo[0]) / parameters.pixelRatio,
                                                           (vertexPatternTo[3] - vertexPatternTo[1]) / parameters.pixelRatio};
                const std::array<float, 2> patternSizeA = {fillPatternFromScale * displaySizeA[0], fillPatternFromScale * displaySizeA[1]};
                const std::array<float, 2> patternSizeB = {fillPatternToScale * displaySizeB[0], fillPatternToScale * displaySizeB[1]};
                const auto posA = getPatternPos(fillPatternPixelCoordUpper, fillPatternPixelCoordLower, patternSizeA, fillPatternTileRatio, patternPos);
                const auto posB = getPatternPos(fillPatternPixelCoordUpper, fillPatternPixelCoordLower, patternSizeB, fillPatternTileRatio, patternPos);
                meshVertices[i].fillPatternPosA[0] = posA[0];
                meshVertices[i].fillPatternPosA[1] = posA[1];
                meshVertices[i].fillPatternPosB[0] = posB[0];
                meshVertices[i].fillPatternPosB[1] = posB[1];
            }
        } else if (fillPatternDrawable || fillOutlinePatternDrawable) {
            auto vertexPatternFrom = fillPatternFrom;
            auto vertexPatternTo = fillPatternTo;
            fillPatternFromReader.read(static_cast<std::uint16_t>(i), vertexPatternFrom);
            fillPatternToReader.read(static_cast<std::uint16_t>(i), vertexPatternTo);
            std::copy(vertexPatternFrom.begin(), vertexPatternFrom.end(), meshVertices[i].fillPatternFrom);
            std::copy(vertexPatternTo.begin(), vertexPatternTo.end(), meshVertices[i].fillPatternTo);
        } else if (circleDrawable) {
            const auto encodedX = meshVertices[i].position[0];
            const auto encodedY = meshVertices[i].position[1];
            meshVertices[i].position[0] = std::floor(encodedX * 0.5f);
            meshVertices[i].position[1] = std::floor(encodedY * 0.5f);
            meshVertices[i].circleExtrude[0] = std::fmod(encodedX, 2.0f) * 2.0f - 1.0f;
            meshVertices[i].circleExtrude[1] = std::fmod(encodedY, 2.0f) * 2.0f - 1.0f;

            auto vertexColor = circleColor;
            auto vertexStrokeColor = circleStrokeColor;
            auto vertexRadius = circleRadius;
            auto vertexBlur = circleBlur;
            auto vertexOpacity = circleOpacity;
            auto vertexStrokeWidth = circleStrokeWidth;
            auto vertexStrokeOpacity = circleStrokeOpacity;
            std::array<float, 4> packedColor;
            if (circleColorReader.read(static_cast<std::uint16_t>(i), packedColor)) vertexColor = unpackMixColor(packedColor, circleColorT);
            std::array<float, 2> packedRadius;
            if (circleRadiusReader.read(static_cast<std::uint16_t>(i), packedRadius)) vertexRadius = unpackMixFloat(packedRadius, circleRadiusT);
            std::array<float, 2> packedBlur;
            if (circleBlurReader.read(static_cast<std::uint16_t>(i), packedBlur)) vertexBlur = unpackMixFloat(packedBlur, circleBlurT);
            std::array<float, 2> packedOpacity;
            if (circleOpacityReader.read(static_cast<std::uint16_t>(i), packedOpacity)) vertexOpacity = unpackMixFloat(packedOpacity, circleOpacityT);
            std::array<float, 4> packedStrokeColor;
            if (circleStrokeColorReader.read(static_cast<std::uint16_t>(i), packedStrokeColor)) vertexStrokeColor = unpackMixColor(packedStrokeColor, circleStrokeColorT);
            std::array<float, 2> packedStrokeWidth;
            if (circleStrokeWidthReader.read(static_cast<std::uint16_t>(i), packedStrokeWidth)) vertexStrokeWidth = unpackMixFloat(packedStrokeWidth, circleStrokeWidthT);
            std::array<float, 2> packedStrokeOpacity;
            if (circleStrokeOpacityReader.read(static_cast<std::uint16_t>(i), packedStrokeOpacity)) vertexStrokeOpacity = unpackMixFloat(packedStrokeOpacity, circleStrokeOpacityT);

            meshVertices[i].circleColor[0] = vertexColor.fR;
            meshVertices[i].circleColor[1] = vertexColor.fG;
            meshVertices[i].circleColor[2] = vertexColor.fB;
            meshVertices[i].circleColor[3] = vertexColor.fA;
            meshVertices[i].circleStrokeColor[0] = vertexStrokeColor.fR;
            meshVertices[i].circleStrokeColor[1] = vertexStrokeColor.fG;
            meshVertices[i].circleStrokeColor[2] = vertexStrokeColor.fB;
            meshVertices[i].circleStrokeColor[3] = vertexStrokeColor.fA * vertexStrokeOpacity;
            meshVertices[i].circleData[0] = vertexRadius;
            meshVertices[i].circleData[1] = vertexBlur;
            meshVertices[i].circleData[2] = vertexOpacity;
            meshVertices[i].circleData[3] = vertexStrokeWidth;
        } else if (lineDrawable) {
            std::array<std::uint8_t, 4> lineData;
            if (!lineDataReader.read(static_cast<std::uint16_t>(i), lineData)) {
                return;
            }
            auto vertexLineWidth = lineWidth;
            auto vertexLineGapWidth = lineGapWidth;
            auto vertexLineBlur = lineBlur;
            auto vertexLineOffset = lineOffset;
            auto vertexLineFloorWidth = lineFloorWidth;
            std::array<float, 2> packedLineWidth;
            if (lineWidthReader.read(static_cast<std::uint16_t>(i), packedLineWidth)) {
                vertexLineWidth = unpackMixFloat(packedLineWidth, lineWidthT);
            }
            std::array<float, 2> packedLineGapWidth;
            if (lineGapWidthReader.read(static_cast<std::uint16_t>(i), packedLineGapWidth)) {
                vertexLineGapWidth = unpackMixFloat(packedLineGapWidth, lineGapWidthT);
            }
            std::array<float, 2> packedLineBlur;
            if (lineBlurReader.read(static_cast<std::uint16_t>(i), packedLineBlur)) {
                vertexLineBlur = unpackMixFloat(packedLineBlur, lineBlurT);
            }
            std::array<float, 2> packedLineOffset;
            if (lineOffsetReader.read(static_cast<std::uint16_t>(i), packedLineOffset)) {
                vertexLineOffset = unpackMixFloat(packedLineOffset, lineOffsetT) * -1.0f;
            }
            std::array<float, 2> packedLineFloorWidth;
            if (lineFloorWidthReader.read(static_cast<std::uint16_t>(i), packedLineFloorWidth)) {
                vertexLineFloorWidth = unpackMixFloat(packedLineFloorWidth, lineFloorWidthT);
            }
            const auto normal = lineNormalFromPosNormal(meshVertices[i].position[0], meshVertices[i].position[1]);
            const auto width = lineWidthPair(vertexLineWidth, vertexLineGapWidth);
            const auto pos = linePositionFromPosNormal(meshVertices[i].position[0], meshVertices[i].position[1]);
            const auto extruded = lineExtrudePosition(
                pos[0], pos[1], lineData, vertexLineWidth, vertexLineGapWidth, vertexLineOffset, normal[1], lineRatio);
            meshVertices[i].position[0] = extruded[0];
            meshVertices[i].position[1] = extruded[1];
            meshVertices[i].lineNormal[0] = normal[0];
            meshVertices[i].lineNormal[1] = normal[1];
            meshVertices[i].lineWidth[0] = width[0];
            meshVertices[i].lineWidth[1] = width[1];
            meshVertices[i].lineBlur = vertexLineBlur;
            meshVertices[i].lineProgress = (std::floor(static_cast<float>(lineData[2]) / 4.0f) +
                                            static_cast<float>(lineData[3]) * 64.0f) *
                                           2.0f / 32767.0f;
            meshVertices[i].lineFloorWidth = vertexLineFloorWidth == 0.0f ? 1.0f : vertexLineFloorWidth;
        } else {
            meshVertices[i].lineNormal[0] = 0.0f;
            meshVertices[i].lineNormal[1] = 0.0f;
            meshVertices[i].lineWidth[0] = 0.0f;
            meshVertices[i].lineWidth[1] = 0.0f;
            meshVertices[i].lineBlur = 0.0f;
            meshVertices[i].lineProgress = 0.0f;
            meshVertices[i].lineFloorWidth = 1.0f;
        }
        meshVertices[i].color[0] = color.fR;
        meshVertices[i].color[1] = color.fG;
        meshVertices[i].color[2] = color.fB;
        meshVertices[i].color[3] = color.fA;
    }

    if (const auto& attrs = getVertexAttributes()) {
        Float4Reader colorReader;
        Float2Reader opacityReader;
        const auto colorAttributeId = lineDrawable ? static_cast<std::size_t>(shaders::idLineColorVertexAttribute)
                                      : (fillExtrusionDrawable ? static_cast<std::size_t>(shaders::idFillExtrusionColorVertexAttribute)
                                         : (fillOutlineDrawable ? static_cast<std::size_t>(shaders::idFillOutlineColorVertexAttribute)
                                                                : static_cast<std::size_t>(shaders::idFillColorVertexAttribute)));
        const auto opacityAttributeId = lineDrawable ? static_cast<std::size_t>(shaders::idLineOpacityVertexAttribute)
                                                     : static_cast<std::size_t>(shaders::idFillOpacityVertexAttribute);
        if (const auto& colorAttr = attrs->get(colorAttributeId)) {
            colorReader.attribute = colorAttr.get();
            if (colorAttr->getSharedRawData() && (colorAttr->getSharedType() == gfx::AttributeDataType::Float4 ||
                                                  colorAttr->getSharedType() == gfx::AttributeDataType::Float2)) {
                const auto offset = colorAttr->getSharedOffset() + colorAttr->getSharedVertexOffset() * colorAttr->getSharedStride();
                colorReader.data = static_cast<const std::uint8_t*>(colorAttr->getSharedRawData()->getRawData()) + offset;
                colorReader.stride = colorAttr->getSharedStride();
                colorReader.components = colorAttr->getSharedType() == gfx::AttributeDataType::Float2 ? 2 : 4;
                colorReader.count = colorAttr->getSharedRawData()->getRawCount() - colorAttr->getSharedVertexOffset();
            } else if ((colorAttr->getDataType() == gfx::AttributeDataType::Float4 ||
                        colorAttr->getDataType() == gfx::AttributeDataType::Float2) &&
                       !colorAttr->getRawData().empty()) {
                colorReader.data = colorAttr->getRawData().data();
                colorReader.components = colorAttr->getDataType() == gfx::AttributeDataType::Float2 ? 2 : 4;
                colorReader.stride = sizeof(float) * colorReader.components;
                colorReader.count = colorAttr->getRawData().size() / colorReader.stride;
            }
        }
        if (const auto& opacityAttr = attrs->get(opacityAttributeId)) {
            opacityReader.attribute = opacityAttr.get();
            if (opacityAttr->getSharedRawData() && (opacityAttr->getSharedType() == gfx::AttributeDataType::Float2 ||
                                                    opacityAttr->getSharedType() == gfx::AttributeDataType::Float)) {
                const auto offset = opacityAttr->getSharedOffset() + opacityAttr->getSharedVertexOffset() * opacityAttr->getSharedStride();
                opacityReader.data = static_cast<const std::uint8_t*>(opacityAttr->getSharedRawData()->getRawData()) + offset;
                opacityReader.stride = opacityAttr->getSharedStride();
                opacityReader.components = opacityAttr->getSharedType() == gfx::AttributeDataType::Float ? 1 : 2;
                opacityReader.count = opacityAttr->getSharedRawData()->getRawCount() - opacityAttr->getSharedVertexOffset();
            } else if ((opacityAttr->getDataType() == gfx::AttributeDataType::Float2 ||
                        opacityAttr->getDataType() == gfx::AttributeDataType::Float) &&
                       !opacityAttr->getRawData().empty()) {
                opacityReader.data = opacityAttr->getRawData().data();
                opacityReader.components = opacityAttr->getDataType() == gfx::AttributeDataType::Float ? 1 : 2;
                opacityReader.stride = sizeof(float) * opacityReader.components;
                opacityReader.count = opacityAttr->getRawData().size() / opacityReader.stride;
            }
        }

        for (std::size_t i = 0; i < meshVertices.size(); ++i) {
            auto vertexColor = color;
            std::array<float, 4> packedColor;
            if (colorReader.read(static_cast<std::uint16_t>(i), packedColor)) {
                vertexColor = unpackMixColor(packedColor, colorT);
            }
            if (fillExtrusionDrawable) {
                vertexColor.fA = 1.0f;
            }
            float vertexOpacity = fillExtrusionDrawable ? fillExtrusionOpacity : 1.0f;
            std::array<float, 2> packedOpacity;
            if (opacityReader.read(static_cast<std::uint16_t>(i), packedOpacity)) {
                vertexOpacity = unpackMixFloat(packedOpacity, opacityT);
            }
            if (fillExtrusionDrawable) {
                const auto luminance = vertexColor.fR * 0.2126f + vertexColor.fG * 0.7152f + vertexColor.fB * 0.0722f;
                const auto normalScale = 1.0f / 16384.0f;
                const auto directionalFraction = std::clamp(
                    meshVertices[i].fillExtrusionNormal[0] * normalScale * fillExtrusionLightPosition[0] +
                        meshVertices[i].fillExtrusionNormal[1] * normalScale * fillExtrusionLightPosition[1] +
                        meshVertices[i].fillExtrusionNormal[2] * normalScale * fillExtrusionLightPosition[2],
                    0.0f,
                    1.0f);
                const auto minDirectional = 1.0f - fillExtrusionLightIntensity;
                const auto maxDirectional = std::max(1.0f - luminance + fillExtrusionLightIntensity, 1.0f);
                auto directional = minDirectional + (maxDirectional - minDirectional) * directionalFraction;
                if (meshVertices[i].fillExtrusionNormal[1] != 0.0f) {
                    const auto fMin = 0.7f + (0.98f - 0.7f) * (1.0f - fillExtrusionLightIntensity);
                    const auto factor = std::clamp((meshVertices[i].fillExtrusionT + fillExtrusionBase) * std::pow(fillExtrusionHeight / 150.0f, 0.5f), fMin, 1.0f);
                    directional *= (1.0f - fillExtrusionVerticalGradient) + fillExtrusionVerticalGradient * factor;
                }
                const auto minLightR = 0.3f * (1.0f - fillExtrusionLightColor[0]);
                const auto minLightG = 0.3f * (1.0f - fillExtrusionLightColor[1]);
                const auto minLightB = 0.3f * (1.0f - fillExtrusionLightColor[2]);
                vertexColor.fR = std::clamp((vertexColor.fR + 0.03f) * directional * fillExtrusionLightColor[0], minLightR, 1.0f);
                vertexColor.fG = std::clamp((vertexColor.fG + 0.03f) * directional * fillExtrusionLightColor[1], minLightG, 1.0f);
                vertexColor.fB = std::clamp((vertexColor.fB + 0.03f) * directional * fillExtrusionLightColor[2], minLightB, 1.0f);
            }
            const auto premultiplied = premultiply(vertexColor, vertexOpacity);
            meshVertices[i].color[0] = premultiplied.fR;
            meshVertices[i].color[1] = premultiplied.fG;
            meshVertices[i].color[2] = premultiplied.fB;
            meshVertices[i].color[3] = premultiplied.fA;
        }
    }

    const auto& indexes = sharedIndexes->vector();
    const auto canvasSize = canvas->getBaseLayerSize();
    const float viewport[2] = {static_cast<float>(canvasSize.width()), static_cast<float>(canvasSize.height())};
    if (fillOutlinePatternDrawable) {
        if (!fillImageTexture) {
            return;
        }
        auto& texture = static_cast<Texture2D&>(*fillImageTexture);
        if (texture.needsUpload()) {
            texture.upload();
        }
        const auto& image = texture.getImage();
        if (!image) {
            return;
        }

        const auto makePatternShader = [&](const std::array<float, 4>& pattern) -> sk_sp<SkShader> {
            const auto left = std::clamp(static_cast<int>(std::floor(pattern[0])), 0, image->width());
            const auto top = std::clamp(static_cast<int>(std::floor(pattern[1])), 0, image->height());
            const auto right = std::clamp(static_cast<int>(std::ceil(pattern[2])), left, image->width());
            const auto bottom = std::clamp(static_cast<int>(std::ceil(pattern[3])), top, image->height());
            if (right <= left || bottom <= top) {
                return nullptr;
            }
            const auto subset = image->makeSubset(nullptr, SkIRect::MakeLTRB(left, top, right, bottom), SkImage::RequiredProperties{});
            if (!subset) {
                return nullptr;
            }
            SkMatrix localMatrix;
            localMatrix.setScale(parameters.pixelRatio, parameters.pixelRatio);
            return subset->makeShader(SkTileMode::kRepeat,
                                      SkTileMode::kRepeat,
                                      SkSamplingOptions(SkFilterMode::kLinear),
                                      &localMatrix);
        };

        const auto patternShader = makePatternShader(fillPatternFade < 0.5f ? fillPatternFrom : fillPatternTo);
        if (!patternShader) {
            return;
        }

        SkPaint linePaint;
        linePaint.setAntiAlias(true);
        linePaint.setStyle(SkPaint::kStroke_Style);
        linePaint.setShader(patternShader);
        linePaint.setAlphaf(std::clamp(color.fA, 0.0f, 1.0f));
        for (const auto& segment : segments) {
            if (!segment || segment->getMode().type != gfx::DrawModeType::Lines) {
                continue;
            }
            linePaint.setStrokeWidth(std::max(1.0f, segment->getMode().size));
            const auto& seg = segment->getSegment();
            const auto end = std::min(seg.indexOffset + seg.indexLength, indexes.size());
            for (std::size_t index = seg.indexOffset; index + 1 < end; index += 2) {
                const auto i0 = indexes[index];
                const auto i1 = indexes[index + 1];
                if (i0 >= meshVertices.size() || i1 >= meshVertices.size()) {
                    continue;
                }
                const auto p0 = projectToScreen(matrix, viewport, meshVertices[i0].position[0], meshVertices[i0].position[1]);
                const auto p1 = projectToScreen(matrix, viewport, meshVertices[i1].position[0], meshVertices[i1].position[1]);
                canvas->drawLine(p0[0], p0[1], p1[0], p1[1], linePaint);
            }
        }
        return;
    }

    if (fillOutlineDrawable) {
        SkPaint linePaint;
        linePaint.setAntiAlias(true);
        linePaint.setStyle(SkPaint::kStroke_Style);
        for (const auto& segment : segments) {
            if (!segment || segment->getMode().type != gfx::DrawModeType::Lines) {
                continue;
            }
            linePaint.setStrokeWidth(std::max(1.0f, segment->getMode().size));
            const auto& seg = segment->getSegment();
            const auto end = std::min(seg.indexOffset + seg.indexLength, indexes.size());
            for (std::size_t index = seg.indexOffset; index + 1 < end; index += 2) {
                const auto i0 = indexes[index];
                const auto i1 = indexes[index + 1];
                if (i0 >= meshVertices.size() || i1 >= meshVertices.size()) {
                    continue;
                }
                const auto color0 = SkColor4f{meshVertices[i0].color[0],
                                               meshVertices[i0].color[1],
                                               meshVertices[i0].color[2],
                                               meshVertices[i0].color[3]};
                const auto paintColor = color0.fA > 0.0f ? SkColor4f{color0.fR / color0.fA,
                                                                     color0.fG / color0.fA,
                                                                     color0.fB / color0.fA,
                                                                     color0.fA}
                                                        : color0;
                linePaint.setColor4f(paintColor);
                const auto p0 = projectToScreen(matrix, viewport, meshVertices[i0].position[0], meshVertices[i0].position[1]);
                const auto p1 = projectToScreen(matrix, viewport, meshVertices[i1].position[0], meshVertices[i1].position[1]);
                canvas->drawLine(p0[0], p0[1], p1[0], p1[1], linePaint);
            }
        }
        return;
    }

    if (collisionBoxDrawable) {
        SkPaint linePaint;
        linePaint.setAntiAlias(true);
        linePaint.setStyle(SkPaint::kStroke_Style);
        for (const auto& segment : segments) {
            if (!segment || segment->getMode().type != gfx::DrawModeType::Lines) {
                continue;
            }
            linePaint.setStrokeWidth(std::max(1.0f, segment->getMode().size));
            const auto& seg = segment->getSegment();
            const auto end = std::min(seg.indexOffset + seg.indexLength, indexes.size());
            for (std::size_t index = seg.indexOffset; index + 1 < end; index += 2) {
                const auto i0 = indexes[index];
                const auto i1 = indexes[index + 1];
                if (i0 >= meshVertices.size() || i1 >= meshVertices.size()) {
                    continue;
                }
                const auto placed = meshVertices[i0].collisionPlaced[0] > 0.5f;
                const auto notUsed = meshVertices[i0].collisionPlaced[1] > 0.5f;
                auto boxColor = placed ? SkColor4f{0.0f, 0.0f, 1.0f, 0.25f} : SkColor4f{1.0f, 0.0f, 0.0f, 0.5f};
                if (notUsed) {
                    boxColor.fA *= 0.1f;
                }
                linePaint.setColor4f(boxColor);
                const auto p0 = collisionPointToScreen(matrix,
                                                       viewport,
                                                       parameters.state.getCameraToCenterDistance(),
                                                       collisionExtrudeScale,
                                                       meshVertices[i0]);
                const auto p1 = collisionPointToScreen(matrix,
                                                       viewport,
                                                       parameters.state.getCameraToCenterDistance(),
                                                       collisionExtrudeScale,
                                                       meshVertices[i1]);
                canvas->drawLine(p0[0], p0[1], p1[0], p1[1], linePaint);
            }
        }
        return;
    }

    auto* directContext = static_cast<RendererBackend&>(parameters.backend).getDirectContext();
    const auto vertexBuffer = SkMeshes::MakeVertexBuffer(directContext,
                                                         meshVertices.data(),
                                                         meshVertices.size() * sizeof(MeshVertex));
    const auto indexBuffer = SkMeshes::MakeIndexBuffer(directContext, indexes.data(), indexes.size() * sizeof(std::uint16_t));
    if (!vertexBuffer || !indexBuffer) {
        return;
    }

    auto uniforms = SkData::MakeUninitialized(specification->uniformSize());
    std::memset(uniforms->writable_data(), 0, uniforms->size());
    writeUniform(uniforms, *specification, "u_matrix", matrix.data(), matrix.size() * sizeof(float));
    writeUniform(uniforms, *specification, "u_viewport", viewport, sizeof(viewport));
    if (symbolIconDrawable || symbolSDFDrawable || symbolTextAndIconDrawable) {
        const float cameraToCenterDistance = parameters.state.getCameraToCenterDistance();
        const float symbolFadeChange = parameters.symbolFadeChange;
        const float aspectRatio = parameters.state.getSize().aspectRatio();
        writeUniform(uniforms, *specification, "u_label_plane_matrix", symbolLabelPlaneMatrix.data(), symbolLabelPlaneMatrix.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_coord_matrix", symbolCoordMatrix.data(), symbolCoordMatrix.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_camera_to_center_distance", &cameraToCenterDistance, sizeof(cameraToCenterDistance));
        writeUniform(uniforms, *specification, "u_symbol_fade_change", &symbolFadeChange, sizeof(symbolFadeChange));
        writeUniform(uniforms, *specification, "u_aspect_ratio", &aspectRatio, sizeof(aspectRatio));
        writeUniform(uniforms, *specification, "u_rotate_symbol", &symbolRotateSymbol, sizeof(symbolRotateSymbol));
        writeUniform(uniforms, *specification, "u_pitch_with_map", &symbolPitchWithMap, sizeof(symbolPitchWithMap));
        writeUniform(uniforms, *specification, "u_is_size_zoom_constant", &symbolIsSizeZoomConstant, sizeof(symbolIsSizeZoomConstant));
        writeUniform(uniforms, *specification, "u_is_size_feature_constant", &symbolIsSizeFeatureConstant, sizeof(symbolIsSizeFeatureConstant));
        writeUniform(uniforms, *specification, "u_is_offset", &symbolIsOffset, sizeof(symbolIsOffset));
        writeUniform(uniforms, *specification, "u_size_t", &symbolSizeT, sizeof(symbolSizeT));
        writeUniform(uniforms, *specification, "u_size", &symbolSize, sizeof(symbolSize));
    }
    if (symbolIconDrawable) {
        writeUniform(uniforms, *specification, "u_opacity_t", &symbolOpacityT, sizeof(symbolOpacityT));
        writeUniform(uniforms, *specification, "u_texsize", symbolTexsize.data(), symbolTexsize.size() * sizeof(float));
    } else if (symbolSDFDrawable || symbolTextAndIconDrawable) {
        const float cameraToCenterDistance = parameters.state.getCameraToCenterDistance();
        const float symbolFadeChange = parameters.symbolFadeChange;
        const float aspectRatio = parameters.state.getSize().aspectRatio();
        const float devicePixelRatio = parameters.pixelRatio;
        writeUniform(uniforms, *specification, "u_label_plane_matrix", symbolLabelPlaneMatrix.data(), symbolLabelPlaneMatrix.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_coord_matrix", symbolCoordMatrix.data(), symbolCoordMatrix.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_camera_to_center_distance", &cameraToCenterDistance, sizeof(cameraToCenterDistance));
        writeUniform(uniforms, *specification, "u_symbol_fade_change", &symbolFadeChange, sizeof(symbolFadeChange));
        writeUniform(uniforms, *specification, "u_aspect_ratio", &aspectRatio, sizeof(aspectRatio));
        writeUniform(uniforms, *specification, "u_rotate_symbol", &symbolRotateSymbol, sizeof(symbolRotateSymbol));
        writeUniform(uniforms, *specification, "u_pitch_with_map", &symbolPitchWithMap, sizeof(symbolPitchWithMap));
        writeUniform(uniforms, *specification, "u_is_text", &symbolIsText, sizeof(symbolIsText));
        writeUniform(uniforms, *specification, "u_is_size_zoom_constant", &symbolIsSizeZoomConstant, sizeof(symbolIsSizeZoomConstant));
        writeUniform(uniforms, *specification, "u_is_size_feature_constant", &symbolIsSizeFeatureConstant, sizeof(symbolIsSizeFeatureConstant));
        writeUniform(uniforms, *specification, "u_is_offset", &symbolIsOffset, sizeof(symbolIsOffset));
        writeUniform(uniforms, *specification, "u_size_t", &symbolSizeT, sizeof(symbolSizeT));
        writeUniform(uniforms, *specification, "u_size", &symbolSize, sizeof(symbolSize));
        writeUniform(uniforms, *specification, "u_fill_color_t", &symbolFillColorT, sizeof(symbolFillColorT));
        writeUniform(uniforms, *specification, "u_halo_color_t", &symbolHaloColorT, sizeof(symbolHaloColorT));
        writeUniform(uniforms, *specification, "u_opacity_t", &symbolOpacityT, sizeof(symbolOpacityT));
        writeUniform(uniforms, *specification, "u_halo_width_t", &symbolHaloWidthT, sizeof(symbolHaloWidthT));
        writeUniform(uniforms, *specification, "u_halo_blur_t", &symbolHaloBlurT, sizeof(symbolHaloBlurT));
        writeUniform(uniforms, *specification, "u_device_pixel_ratio", &devicePixelRatio, sizeof(devicePixelRatio));
        writeUniform(uniforms, *specification, "u_is_halo", &symbolIsHalo, sizeof(symbolIsHalo));
        writeUniform(uniforms, *specification, "u_gamma_scale", &symbolGammaScale, sizeof(symbolGammaScale));
    } else if (collisionCircleDrawable) {
        const float cameraToCenterDistance = parameters.state.getCameraToCenterDistance();
        writeUniform(uniforms, *specification, "u_extrude_scale", collisionExtrudeScale.data(), collisionExtrudeScale.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_camera_to_center_distance", &cameraToCenterDistance, sizeof(cameraToCenterDistance));
        writeUniform(uniforms, *specification, "u_overscale_factor", &collisionOverscaleFactor, sizeof(collisionOverscaleFactor));
    } else if (circleDrawable) {
        const float cameraToCenterDistance = parameters.state.getCameraToCenterDistance();
        writeUniform(uniforms, *specification, "u_extrude_scale", circleExtrudeScale.data(), circleExtrudeScale.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_camera_to_center_distance", &cameraToCenterDistance, sizeof(cameraToCenterDistance));
        writeUniform(uniforms, *specification, "u_scale_with_map", &circleScaleWithMap, sizeof(circleScaleWithMap));
        writeUniform(uniforms, *specification, "u_pitch_with_map", &circlePitchWithMap, sizeof(circlePitchWithMap));
    } else if (heatmapDrawable) {
        writeUniform(uniforms, *specification, "u_extrude_scale", &heatmapExtrudeScale, sizeof(heatmapExtrudeScale));
        writeUniform(uniforms, *specification, "u_weight", &heatmapWeight, sizeof(heatmapWeight));
        writeUniform(uniforms, *specification, "u_radius", &heatmapRadius, sizeof(heatmapRadius));
        writeUniform(uniforms, *specification, "u_intensity", &heatmapIntensity, sizeof(heatmapIntensity));
        writeUniform(uniforms, *specification, "u_weight_t", &heatmapWeightT, sizeof(heatmapWeightT));
        writeUniform(uniforms, *specification, "u_radius_t", &heatmapRadiusT, sizeof(heatmapRadiusT));
    } else if (heatmapTextureDrawable) {
        writeUniform(uniforms, *specification, "u_world_size", viewport, sizeof(viewport));
        writeUniform(uniforms, *specification, "u_opacity", &heatmapTextureOpacity, sizeof(heatmapTextureOpacity));
    } else if (hillshadePrepareDrawable) {
        writeUniform(uniforms, *specification, "u_unpack", hillshadeUnpack.data(), hillshadeUnpack.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_dimension", hillshadeDimension.data(), hillshadeDimension.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_zoom", &hillshadeZoom, sizeof(hillshadeZoom));
    } else if (hillshadeDrawable) {
        writeUniform(uniforms, *specification, "u_unpack", hillshadeUnpack.data(), hillshadeUnpack.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_zoom", &hillshadeZoom, sizeof(hillshadeZoom));
        writeUniform(uniforms, *specification, "u_latrange", hillshadeLatrange.data(), hillshadeLatrange.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_exaggeration", &hillshadeExaggeration, sizeof(hillshadeExaggeration));
        writeUniform(uniforms, *specification, "u_method", &hillshadeMethod, sizeof(hillshadeMethod));
        writeUniform(uniforms, *specification, "u_num_lights", &hillshadeNumLights, sizeof(hillshadeNumLights));
        writeUniform(uniforms, *specification, "u_accent", hillshadeAccent.data(), hillshadeAccent.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_altitudes", hillshadeAltitudes.data(), hillshadeAltitudes.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_azimuths", hillshadeAzimuths.data(), hillshadeAzimuths.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_shadow0", hillshadeShadows.data(), sizeof(float) * 4);
        writeUniform(uniforms, *specification, "u_shadow1", hillshadeShadows.data() + 4, sizeof(float) * 4);
        writeUniform(uniforms, *specification, "u_shadow2", hillshadeShadows.data() + 8, sizeof(float) * 4);
        writeUniform(uniforms, *specification, "u_shadow3", hillshadeShadows.data() + 12, sizeof(float) * 4);
        writeUniform(uniforms, *specification, "u_highlight0", hillshadeHighlights.data(), sizeof(float) * 4);
        writeUniform(uniforms, *specification, "u_highlight1", hillshadeHighlights.data() + 4, sizeof(float) * 4);
        writeUniform(uniforms, *specification, "u_highlight2", hillshadeHighlights.data() + 8, sizeof(float) * 4);
        writeUniform(uniforms, *specification, "u_highlight3", hillshadeHighlights.data() + 12, sizeof(float) * 4);
    } else if (colorReliefDrawable) {
        writeUniform(uniforms, *specification, "u_unpack", colorReliefUnpack.data(), colorReliefUnpack.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_dimension", colorReliefDimension.data(), colorReliefDimension.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_color_ramp_size", &colorReliefRampSize, sizeof(colorReliefRampSize));
        writeUniform(uniforms, *specification, "u_opacity", &colorReliefOpacity, sizeof(colorReliefOpacity));
        const float defaultElevationMin = 0.0f;
        const float defaultElevationScale = 1.0f;
        writeUniform(uniforms, *specification, "u_elevation_min", &defaultElevationMin, sizeof(defaultElevationMin));
        writeUniform(uniforms, *specification, "u_elevation_scale", &defaultElevationScale, sizeof(defaultElevationScale));
    } else if (rasterDrawable) {
        writeUniform(uniforms, *specification, "u_tl_parent", rasterTlParent.data(), rasterTlParent.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_scale_parent", &rasterScaleParent, sizeof(rasterScaleParent));
        writeUniform(uniforms, *specification, "u_buffer_scale", &rasterBufferScale, sizeof(rasterBufferScale));
        writeUniform(uniforms, *specification, "u_spin_weights", rasterSpinWeights.data(), rasterSpinWeights.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_fade_t", &rasterFadeT, sizeof(rasterFadeT));
        writeUniform(uniforms, *specification, "u_opacity", &rasterOpacity, sizeof(rasterOpacity));
        writeUniform(uniforms, *specification, "u_brightness_low", &rasterBrightnessLow, sizeof(rasterBrightnessLow));
        writeUniform(uniforms, *specification, "u_brightness_high", &rasterBrightnessHigh, sizeof(rasterBrightnessHigh));
        writeUniform(uniforms, *specification, "u_saturation_factor", &rasterSaturationFactor, sizeof(rasterSaturationFactor));
        writeUniform(uniforms, *specification, "u_contrast_factor", &rasterContrastFactor, sizeof(rasterContrastFactor));
    } else if (fillExtrusionPatternDrawable) {
        writeUniform(uniforms, *specification, "u_fade", &fillPatternFade, sizeof(fillPatternFade));
        writeUniform(uniforms, *specification, "u_opacity", &fillExtrusionOpacity, sizeof(fillExtrusionOpacity));
    } else if (fillPatternDrawable) {
        const float pixelRatio = parameters.pixelRatio;
        writeUniform(uniforms, *specification, "u_pixel_coord_upper", fillPatternPixelCoordUpper.data(), fillPatternPixelCoordUpper.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_pixel_coord_lower", fillPatternPixelCoordLower.data(), fillPatternPixelCoordLower.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_tile_ratio", &fillPatternTileRatio, sizeof(fillPatternTileRatio));
        writeUniform(uniforms, *specification, "u_pattern_from", fillPatternFrom.data(), fillPatternFrom.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_pattern_to", fillPatternTo.data(), fillPatternTo.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_texsize", fillPatternTexsize.data(), fillPatternTexsize.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_fade", &fillPatternFade, sizeof(fillPatternFade));
        writeUniform(uniforms, *specification, "u_from_scale", &fillPatternFromScale, sizeof(fillPatternFromScale));
        writeUniform(uniforms, *specification, "u_to_scale", &fillPatternToScale, sizeof(fillPatternToScale));
        writeUniform(uniforms, *specification, "u_pixel_ratio", &pixelRatio, sizeof(pixelRatio));
    } else if (backgroundPatternDrawable) {
        writeUniform(uniforms, *specification, "u_pixel_coord_upper", backgroundPatternPixelCoordUpper.data(), backgroundPatternPixelCoordUpper.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_pixel_coord_lower", backgroundPatternPixelCoordLower.data(), backgroundPatternPixelCoordLower.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_tile_units_to_pixels", &backgroundPatternTileUnitsToPixels, sizeof(backgroundPatternTileUnitsToPixels));
        writeUniform(uniforms, *specification, "u_pattern_tl_a", backgroundPatternTlA.data(), backgroundPatternTlA.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_pattern_br_a", backgroundPatternBrA.data(), backgroundPatternBrA.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_pattern_tl_b", backgroundPatternTlB.data(), backgroundPatternTlB.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_pattern_br_b", backgroundPatternBrB.data(), backgroundPatternBrB.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_pattern_size_a", backgroundPatternSizeA.data(), backgroundPatternSizeA.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_pattern_size_b", backgroundPatternSizeB.data(), backgroundPatternSizeB.size() * sizeof(float));
        writeUniform(uniforms, *specification, "u_scale_a", &backgroundPatternScaleA, sizeof(backgroundPatternScaleA));
        writeUniform(uniforms, *specification, "u_scale_b", &backgroundPatternScaleB, sizeof(backgroundPatternScaleB));
        writeUniform(uniforms, *specification, "u_mix", &backgroundPatternMix, sizeof(backgroundPatternMix));
        writeUniform(uniforms, *specification, "u_opacity", &backgroundPatternOpacity, sizeof(backgroundPatternOpacity));
    }

    std::array<SkMesh::ChildPtr, 3> children;
    std::size_t childCount = 0;
    if (symbolTextAndIconDrawable) {
        if (!symbolImageTexture || !symbolImageIconTexture) {
            return;
        }
        auto& glyphTexture = static_cast<Texture2D&>(*symbolImageTexture);
        auto& iconTexture = static_cast<Texture2D&>(*symbolImageIconTexture);
        if (glyphTexture.needsUpload()) {
            glyphTexture.upload();
        }
        if (iconTexture.needsUpload()) {
            iconTexture.upload();
        }
        const auto& glyphImage = glyphTexture.getImage();
        const auto& iconImage = iconTexture.getImage();
        if (!glyphImage || !iconImage) {
            return;
        }
        const auto glyphFilterMode = glyphTexture.getSamplerState().filter == gfx::TextureFilterType::Nearest ? SkFilterMode::kNearest
                                                                                                              : SkFilterMode::kLinear;
        const auto iconFilterMode = iconTexture.getSamplerState().filter == gfx::TextureFilterType::Nearest ? SkFilterMode::kNearest
                                                                                                            : SkFilterMode::kLinear;
        auto glyphShader = glyphImage->makeShader(SkTileMode::kClamp,
                                                  SkTileMode::kClamp,
                                                  SkSamplingOptions(glyphFilterMode));
        auto iconShader = iconImage->makeShader(SkTileMode::kClamp,
                                                SkTileMode::kClamp,
                                                SkSamplingOptions(iconFilterMode));
        if (!glyphShader || !iconShader) {
            return;
        }
        children[0] = std::move(glyphShader);
        children[1] = std::move(iconShader);
        childCount = 2;
    } else if (symbolIconDrawable || symbolSDFDrawable) {
        const auto& texturePtr = symbolImageTexture ? symbolImageTexture : symbolImageIconTexture;
        if (!texturePtr) {
            return;
        }
        auto& texture = static_cast<Texture2D&>(*texturePtr);
        if (texture.needsUpload()) {
            texture.upload();
        }
        const auto& image = texture.getImage();
        if (!image) {
            return;
        }
        const auto filterMode = texture.getSamplerState().filter == gfx::TextureFilterType::Nearest ? SkFilterMode::kNearest
                                                                                                     : SkFilterMode::kLinear;
        auto imageShader = image->makeShader(SkTileMode::kClamp,
                                             SkTileMode::kClamp,
                                             SkSamplingOptions(filterMode));
        if (!imageShader) {
            return;
        }
        children[0] = std::move(imageShader);
        childCount = 1;
    } else if (heatmapTextureDrawable) {
        if (!heatmapImageTexture || !heatmapColorRampTexture) {
            return;
        }
        auto& imageTexture = static_cast<Texture2D&>(*heatmapImageTexture);
        auto& rampTexture = static_cast<Texture2D&>(*heatmapColorRampTexture);
        if (imageTexture.needsUpload()) imageTexture.upload();
        if (rampTexture.needsUpload()) rampTexture.upload();
        const auto& image = imageTexture.getImage();
        const auto& ramp = rampTexture.getImage();
        if (!image || !ramp) {
            return;
        }
        const float imageSize[2] = {static_cast<float>(image->width()), static_cast<float>(image->height())};
        writeUniform(uniforms, *specification, "u_image_size", imageSize, sizeof(imageSize));
        auto imageShader = image->makeRawShader(SkTileMode::kClamp,
                                                SkTileMode::kClamp,
                                                SkSamplingOptions(SkFilterMode::kLinear));
        auto rampShader = ramp->makeRawShader(SkTileMode::kClamp,
                                              SkTileMode::kClamp,
                                              SkSamplingOptions(SkFilterMode::kLinear));
        if (!imageShader || !rampShader) {
            return;
        }
        children[0] = std::move(imageShader);
        children[1] = std::move(rampShader);
        childCount = 2;
    } else if (hillshadePrepareDrawable || hillshadeDrawable) {
        if (!hillshadeImageTexture) {
            return;
        }
        auto& texture = static_cast<Texture2D&>(*hillshadeImageTexture);
        if (texture.needsUpload()) {
            texture.upload();
        }
        const auto& image = texture.getImage();
        if (!image) {
            return;
        }
        const float dimensions[2] = {static_cast<float>(image->width()), static_cast<float>(image->height())};
        writeUniform(uniforms, *specification, "u_dimension", dimensions, sizeof(dimensions));
        const auto filterMode = hillshadeDrawable && hillshadeOverzoom ? SkFilterMode::kLinear
                                : (texture.getSamplerState().filter == gfx::TextureFilterType::Nearest ? SkFilterMode::kNearest
                                                                                                        : SkFilterMode::kLinear);
        auto imageShader = image->makeRawShader(SkTileMode::kClamp,
                                                SkTileMode::kClamp,
                                                SkSamplingOptions(filterMode));
        if (!imageShader) {
            return;
        }
        children[0] = std::move(imageShader);
        childCount = 1;
    } else if (colorReliefDrawable) {
        if (!colorReliefImageTexture || !colorReliefElevationStopsTexture || !colorReliefColorStopsTexture) {
            return;
        }
        auto& demTexture = static_cast<Texture2D&>(*colorReliefImageTexture);
        auto& elevationTexture = static_cast<Texture2D&>(*colorReliefElevationStopsTexture);
        auto& colorTexture = static_cast<Texture2D&>(*colorReliefColorStopsTexture);
        if (demTexture.needsUpload()) demTexture.upload();
        if (elevationTexture.needsUpload()) elevationTexture.upload();
        if (colorTexture.needsUpload()) colorTexture.upload();
        const auto& demImage = demTexture.getImage();
        const auto& colorImage = colorTexture.getImage();
        if (!demImage || !colorImage) {
            return;
        }
        std::array<float, 256> elevationStops{};
        const auto& elevationPixels = elevationTexture.getPixels();
        const auto stopCount = std::min<std::size_t>(std::max(colorReliefRampSize, 0), elevationStops.size());
        if (!stopCount || elevationPixels.size() < stopCount * sizeof(float) * 4) {
            return;
        }
        if (elevationPixels.size() >= stopCount * sizeof(float) * 4) {
            for (std::size_t i = 0; i < stopCount; ++i) {
                std::memcpy(&elevationStops[i], elevationPixels.data() + i * sizeof(float) * 4, sizeof(float));
            }
        }
        const auto elevationMin = elevationStops[0];
        const auto elevationMax = elevationStops[stopCount - 1];
        const auto elevationDenom = elevationMax - elevationMin;
        const auto elevationScale = std::abs(elevationDenom) < 0.0001f ? 1.0f : 1.0f / elevationDenom;
        writeUniform(uniforms, *specification, "u_elevation_min", &elevationMin, sizeof(elevationMin));
        writeUniform(uniforms, *specification, "u_elevation_scale", &elevationScale, sizeof(elevationScale));
        std::vector<float> normalizedElevationStops(stopCount * 4, 0.0f);
        for (std::size_t i = 0; i < stopCount; ++i) {
            normalizedElevationStops[i * 4] = (elevationStops[i] - elevationMin) * elevationScale;
            normalizedElevationStops[i * 4 + 3] = 1.0f;
        }
        SkPixmap elevationPixmap(SkImageInfo::Make(static_cast<int>(stopCount), 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType),
                                 normalizedElevationStops.data(),
                                 stopCount * sizeof(float) * 4);
        auto normalizedElevationImage = SkImages::RasterFromPixmapCopy(elevationPixmap);
        if (!normalizedElevationImage) {
            return;
        }
        auto demShader = demImage->makeRawShader(SkTileMode::kClamp,
                                                  SkTileMode::kClamp,
                                                  SkSamplingOptions(SkFilterMode::kLinear));
        auto elevationShader = normalizedElevationImage->makeRawShader(SkTileMode::kClamp,
                                                                       SkTileMode::kClamp,
                                                                       SkSamplingOptions(SkFilterMode::kNearest));
        auto colorShader = colorImage->makeShader(SkTileMode::kClamp,
                                                   SkTileMode::kClamp,
                                                   SkSamplingOptions(SkFilterMode::kLinear));
        if (!demShader || !elevationShader || !colorShader) {
            return;
        }
        children[0] = std::move(demShader);
        children[1] = std::move(elevationShader);
        children[2] = std::move(colorShader);
        childCount = 3;
    } else if (rasterDrawable) {
        if (!rasterImage0Texture || !rasterImage1Texture) {
            return;
        }
        auto& texture0 = static_cast<Texture2D&>(*rasterImage0Texture);
        auto& texture1 = static_cast<Texture2D&>(*rasterImage1Texture);
        if (texture0.needsUpload()) {
            texture0.upload();
        }
        if (texture1.needsUpload()) {
            texture1.upload();
        }
        const auto& image0 = texture0.getImage();
        const auto& image1 = texture1.getImage();
        if (!image0 || !image1) {
            return;
        }
        const float texsize0[2] = {static_cast<float>(image0->width()), static_cast<float>(image0->height())};
        const float texsize1[2] = {static_cast<float>(image1->width()), static_cast<float>(image1->height())};
        writeUniform(uniforms, *specification, "u_texsize0", texsize0, sizeof(texsize0));
        writeUniform(uniforms, *specification, "u_texsize1", texsize1, sizeof(texsize1));
        const auto filterMode = texture0.getSamplerState().filter == gfx::TextureFilterType::Nearest ? SkFilterMode::kNearest
                                                                                                      : SkFilterMode::kLinear;
        auto image0Shader = image0->makeShader(SkTileMode::kClamp,
                                               SkTileMode::kClamp,
                                               SkSamplingOptions(filterMode));
        auto image1Shader = image1->makeShader(SkTileMode::kClamp,
                                               SkTileMode::kClamp,
                                               SkSamplingOptions(filterMode));
        if (!image0Shader || !image1Shader) {
            return;
        }
        children[0] = std::move(image0Shader);
        children[1] = std::move(image1Shader);
        childCount = 2;
    } else if (fillExtrusionPatternDrawable) {
        if (!fillExtrusionImageTexture) {
            return;
        }
        auto& texture = static_cast<Texture2D&>(*fillExtrusionImageTexture);
        if (texture.needsUpload()) {
            texture.upload();
        }
        const auto& image = texture.getImage();
        if (!image) {
            return;
        }
        auto imageShader = image->makeShader(SkTileMode::kClamp,
                                             SkTileMode::kClamp,
                                             SkSamplingOptions(SkFilterMode::kLinear));
        if (!imageShader) {
            return;
        }
        children[0] = std::move(imageShader);
        childCount = 1;
    } else if (fillPatternDrawable) {
        if (!fillImageTexture) {
            return;
        }
        auto& texture = static_cast<Texture2D&>(*fillImageTexture);
        if (texture.needsUpload()) {
            texture.upload();
        }
        const auto& image = texture.getImage();
        if (!image) {
            return;
        }
        auto imageShader = image->makeShader(SkTileMode::kClamp,
                                             SkTileMode::kClamp,
                                             SkSamplingOptions(SkFilterMode::kLinear));
        if (!imageShader) {
            return;
        }
        children[0] = std::move(imageShader);
        childCount = 1;
    } else if (backgroundPatternDrawable) {
        if (!backgroundImageTexture) {
            return;
        }
        auto& texture = static_cast<Texture2D&>(*backgroundImageTexture);
        if (texture.needsUpload()) {
            texture.upload();
        }
        const auto& image = texture.getImage();
        if (!image) {
            return;
        }
        auto imageShader = image->makeShader(SkTileMode::kClamp,
                                             SkTileMode::kClamp,
                                             SkSamplingOptions(SkFilterMode::kLinear));
        if (!imageShader) {
            return;
        }
        children[0] = std::move(imageShader);
        childCount = 1;
    } else if (lineGradientDrawable || linePatternDrawable || lineSDFDrawable) {
        if (!lineImageTexture) {
            return;
        }
        auto& texture = static_cast<Texture2D&>(*lineImageTexture);
        if (texture.needsUpload()) {
            texture.upload();
        }
        const auto& image = texture.getImage();
        if (!image) {
            return;
        }
        auto gradientShader = image->makeShader(lineSDFDrawable ? SkTileMode::kRepeat : SkTileMode::kClamp,
                                                SkTileMode::kClamp,
                                                SkSamplingOptions(SkFilterMode::kLinear));
        if (!gradientShader) {
            return;
        }
        children[0] = std::move(gradientShader);
        childCount = 1;

        if (linePatternDrawable) {
            const shaders::LinePatternTilePropsUBO* tileProps = nullptr;
#if MLN_UBO_CONSOLIDATION
            if (const auto* lineTilePropsUnion = getUBO<shaders::LineTilePropsUnionUBO>(
                    layerUniforms, shaders::idLineTilePropsUBO, getUBOIndex())) {
                tileProps = &lineTilePropsUnion->linePatternTilePropsUBO;
            }
#endif
            if (!tileProps) {
                tileProps = getUBO<shaders::LinePatternTilePropsUBO>(&getUniformBuffers(), shaders::idLineTilePropsUBO);
            }
            if (!tileProps) {
                return;
            }
            writeUniform(uniforms, *specification, "u_pattern_from", tileProps->pattern_from.data(), tileProps->pattern_from.size() * sizeof(float));
            writeUniform(uniforms, *specification, "u_pattern_to", tileProps->pattern_to.data(), tileProps->pattern_to.size() * sizeof(float));
            writeUniform(uniforms, *specification, "u_pattern_scale", tileProps->scale.data(), tileProps->scale.size() * sizeof(float));
            writeUniform(uniforms, *specification, "u_texsize", tileProps->texsize.data(), tileProps->texsize.size() * sizeof(float));
            writeUniform(uniforms, *specification, "u_fade", &tileProps->fade, sizeof(tileProps->fade));
        } else if (lineSDFDrawable) {
            const shaders::LineSDFDrawableUBO* sdfDrawable = nullptr;
            const shaders::LineSDFTilePropsUBO* tileProps = nullptr;
#if MLN_UBO_CONSOLIDATION
            if (const auto* lineDrawableUnion = getUBO<shaders::LineDrawableUnionUBO>(
                    layerUniforms, shaders::idLineDrawableUBO, getUBOIndex())) {
                sdfDrawable = &lineDrawableUnion->lineSDFDrawableUBO;
            }
            if (const auto* lineTilePropsUnion = getUBO<shaders::LineTilePropsUnionUBO>(
                    layerUniforms, shaders::idLineTilePropsUBO, getUBOIndex())) {
                tileProps = &lineTilePropsUnion->lineSDFTilePropsUBO;
            }
#endif
            if (!sdfDrawable) {
                sdfDrawable = getUBO<shaders::LineSDFDrawableUBO>(&getUniformBuffers(), shaders::idLineDrawableUBO);
            }
            if (!tileProps) {
                tileProps = getUBO<shaders::LineSDFTilePropsUBO>(&getUniformBuffers(), shaders::idLineTilePropsUBO);
            }
            if (!sdfDrawable || !tileProps) {
                return;
            }
            const float sdfTextureSize[2] = {static_cast<float>(image->width()), static_cast<float>(image->height())};
            writeUniform(uniforms, *specification, "u_patternscale_a", sdfDrawable->patternscale_a.data(), sdfDrawable->patternscale_a.size() * sizeof(float));
            writeUniform(uniforms, *specification, "u_patternscale_b", sdfDrawable->patternscale_b.data(), sdfDrawable->patternscale_b.size() * sizeof(float));
            writeUniform(uniforms, *specification, "u_tex_y_a", &sdfDrawable->tex_y_a, sizeof(sdfDrawable->tex_y_a));
            writeUniform(uniforms, *specification, "u_tex_y_b", &sdfDrawable->tex_y_b, sizeof(sdfDrawable->tex_y_b));
            writeUniform(uniforms, *specification, "u_sdfgamma", &tileProps->sdfgamma, sizeof(tileProps->sdfgamma));
            writeUniform(uniforms, *specification, "u_mix", &tileProps->mix, sizeof(tileProps->mix));
            writeUniform(uniforms, *specification, "u_sdf_texsize", sdfTextureSize, sizeof(sdfTextureSize));
        }
    }

    SkPaint paint;
    paint.setAntiAlias(true);
    if (heatmapDrawable) {
        paint.setBlendMode(SkBlendMode::kPlus);
    }

    for (const auto& segment : segments) {
        if (!segment || segment->getMode().type != gfx::DrawModeType::Triangles) {
            continue;
        }
        const auto& seg = segment->getSegment();
        const auto end = std::min(seg.indexOffset + seg.indexLength, indexes.size());
        if (end <= seg.indexOffset) {
            continue;
        }
        if (seg.vertexOffset >= meshVertices.size()) {
            continue;
        }
        const auto segmentVertexCount = std::min(seg.vertexLength, meshVertices.size() - seg.vertexOffset);
        if (!segmentVertexCount) {
            continue;
        }

        const auto mesh = SkMesh::MakeIndexed(specification,
                                              SkMesh::Mode::kTriangles,
                                              vertexBuffer,
                                              segmentVertexCount,
                                              seg.vertexOffset * sizeof(MeshVertex),
                                              indexBuffer,
                                              end - seg.indexOffset,
                                               seg.indexOffset * sizeof(std::uint16_t),
                                               uniforms,
                                               SkSpan<SkMesh::ChildPtr>(children.data(), childCount),
                                               conservativeMeshBounds());
        if (mesh.mesh.isValid()) {
            canvas->drawMesh(mesh.mesh, SkBlender::Mode(SkBlendMode::kDst), paint);
        }
    }
}

void Drawable::updateVertexAttributes(gfx::VertexAttributeArrayPtr attributes,
                                       std::size_t vertexCount_,
                                       gfx::DrawMode mode,
                                       gfx::IndexVectorBasePtr indexes,
                                       const SegmentBase* segments_,
                                       std::size_t segmentCount) {
    setVertexAttributes(std::move(attributes));
    sharedIndexes = std::move(indexes);
    vertexCount = vertexCount_;
    segments.clear();
    for (std::size_t i = 0; segments_ && i < segmentCount; ++i) {
        auto copy = SegmentBase{segments_[i].vertexOffset,
                                segments_[i].indexOffset,
                                segments_[i].vertexLength,
                                segments_[i].indexLength,
                                segments_[i].sortKey};
        segments.emplace_back(std::make_unique<gfx::Drawable::DrawSegment>(mode, std::move(copy)));
    }
}

void Drawable::setVertices(std::vector<std::uint8_t>&& vertices_, std::size_t vertexCount_, gfx::AttributeDataType type) {
    vertices = std::move(vertices_);
    vertexCount = vertexCount_;
    vertexDataType = type;
}

void Drawable::setIndexData(gfx::IndexVectorBasePtr indexes, std::vector<UniqueDrawSegment> segments_) {
    sharedIndexes = std::move(indexes);
    segments = std::move(segments_);
}

const gfx::UniformBufferArray& Drawable::getUniformBuffers() const {
    return *uniformBuffers;
}

gfx::UniformBufferArray& Drawable::mutableUniformBuffers() {
    return *uniformBuffers;
}

} // namespace skia
} // namespace mbgl
