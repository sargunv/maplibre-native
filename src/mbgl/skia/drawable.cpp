#include "skia_impl.hpp"

#include <mbgl/renderer/paint_parameters.hpp>
#include <mbgl/skia/renderer_backend.hpp>
#include <mbgl/shaders/background_layer_ubo.hpp>
#include <mbgl/shaders/fill_layer_ubo.hpp>
#include <mbgl/shaders/line_layer_ubo.hpp>
#include <mbgl/shaders/shader_defines.hpp>

#include <include/core/SkBlender.h>
#include <include/core/SkColor.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
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

struct MeshVertex {
    float position[2];
    float color[4];
    float lineNormal[2];
    float lineWidth[2];
    float lineBlur;
    float lineProgress;
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

void writeUniform(sk_sp<SkData>& uniforms,
                  const SkMeshSpecification& specification,
                  const char* name,
                  const void* data,
                  const std::size_t size) {
    const auto* uniform = specification.findUniform(name);
    if (!uniform || uniform->offset + size > uniforms->size()) {
        return;
    }
    std::memcpy(static_cast<std::uint8_t*>(uniforms->writable_data()) + uniform->offset, data, size);
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
    float lineWidth = 1.0f;
    float lineGapWidth = 0.0f;
    float lineBlur = 0.0f;
    float lineOffset = 0.0f;
    float lineWidthT = 0.0f;
    float lineGapWidthT = 0.0f;
    float lineBlurT = 0.0f;
    float lineOffsetT = 0.0f;
    float lineRatio = 1.0f;
    bool lineDrawable = false;
    const auto& lineImageTexture = getTexture(shaders::idLineImageTexture);
    const bool lineGradientDrawable = getName().find("lineGradient") != std::string::npos && static_cast<bool>(lineImageTexture);
    bool hasLinePositionAttribute = vertexDataType == gfx::AttributeDataType::Short4;
    if (const auto& attrs = getVertexAttributes()) {
        if (const auto& lineDataAttr = attrs->get(shaders::idLineDataVertexAttribute)) {
            hasLinePositionAttribute = hasLinePositionAttribute &&
                                       (lineDataAttr->getSharedType() == gfx::AttributeDataType::UByte4 ||
                                        lineDataAttr->getDataType() == gfx::AttributeDataType::UByte4);
        }
    }

    if (hasLinePositionAttribute) {
        const shaders::LineDrawableUBO* lineDrawableUBO = nullptr;
        shaders::LineDrawableUBO gradientAsLineDrawableUBO = {};
#if MLN_UBO_CONSOLIDATION
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
            } else {
                lineDrawableUBO = &lineDrawableUnion->lineDrawableUBO;
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
            if (const auto* props = getUBO<shaders::LineEvaluatedPropsUBO>(layerUniforms, shaders::idLineEvaluatedPropsUBO)) {
                color = lineGradientDrawable ? SkColor4f{1.0f, 1.0f, 1.0f, props->opacity}
                                             : toSkColor(props->color, props->opacity);
                lineBlur = props->blur;
                lineOffset = props->offset * -1.0f;
                lineWidth = props->width;
                lineGapWidth = props->gapwidth;
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
        const std::size_t fallbackPositionAttributeId = lineDrawable
                                                            ? static_cast<std::size_t>(shaders::idLinePosNormalVertexAttribute)
                                                            : static_cast<std::size_t>(shaders::idFillPosVertexAttribute);
        const auto& attr = attrs->get(positionAttributeId) ? attrs->get(positionAttributeId)
                                                          : attrs->get(fallbackPositionAttributeId);
        if (attr && attr->getSharedRawData() && attr->getSharedType() == gfx::AttributeDataType::Short2) {
            const auto* raw = static_cast<const std::uint8_t*>(attr->getSharedRawData()->getRawData());
            vertexReader = VertexReader{raw + attr->getSharedOffset() + attr->getSharedVertexOffset() * attr->getSharedStride(),
                                        vertexCount ? vertexCount : attr->getSharedRawData()->getRawCount(),
                                        attr->getSharedStride()};
        } else if (attr && attr->getDataType() == gfx::AttributeDataType::Short2 && !attr->getRawData().empty()) {
            vertexReader = VertexReader{attr->getRawData().data(), vertexCount, sizeof(std::int16_t) * 2};
        }
    }
    if (!vertexReader.data) {
        return;
    }

    const auto specification = lineGradientDrawable ? lineGradientMeshSpecification()
                               : (lineDrawable ? lineMeshSpecification() : solidColorMeshSpecification());
    if (!specification) {
        return;
    }

    if (vertexReader.count > std::numeric_limits<std::uint16_t>::max()) {
        return;
    }

    UByte4Reader lineDataReader;
    Float2Reader lineWidthReader;
    Float2Reader lineGapWidthReader;
    Float2Reader lineBlurReader;
    Float2Reader lineOffsetReader;
    if (lineDrawable) {
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
        }
        if (!lineDataReader.data) {
            return;
        }
    }

    std::vector<MeshVertex> meshVertices(vertexReader.count);
    for (std::size_t i = 0; i < vertexReader.count; ++i) {
        if (!vertexReader.read(static_cast<std::uint16_t>(i), meshVertices[i].position[0], meshVertices[i].position[1])) {
            return;
        }
        if (lineDrawable) {
            std::array<std::uint8_t, 4> lineData;
            if (!lineDataReader.read(static_cast<std::uint16_t>(i), lineData)) {
                return;
            }
            auto vertexLineWidth = lineWidth;
            auto vertexLineGapWidth = lineGapWidth;
            auto vertexLineBlur = lineBlur;
            auto vertexLineOffset = lineOffset;
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
        } else {
            meshVertices[i].lineNormal[0] = 0.0f;
            meshVertices[i].lineNormal[1] = 0.0f;
            meshVertices[i].lineWidth[0] = 0.0f;
            meshVertices[i].lineWidth[1] = 0.0f;
            meshVertices[i].lineBlur = 0.0f;
            meshVertices[i].lineProgress = 0.0f;
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
                                                   : static_cast<std::size_t>(shaders::idFillColorVertexAttribute);
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
            float vertexOpacity = 1.0f;
            std::array<float, 2> packedOpacity;
            if (opacityReader.read(static_cast<std::uint16_t>(i), packedOpacity)) {
                vertexOpacity = unpackMixFloat(packedOpacity, opacityT);
            }
            const auto premultiplied = premultiply(vertexColor, vertexOpacity);
            meshVertices[i].color[0] = premultiplied.fR;
            meshVertices[i].color[1] = premultiplied.fG;
            meshVertices[i].color[2] = premultiplied.fB;
            meshVertices[i].color[3] = premultiplied.fA;
        }
    }

    auto* directContext = static_cast<RendererBackend&>(parameters.backend).getDirectContext();
    const auto& indexes = sharedIndexes->vector();
    const auto vertexBuffer = SkMeshes::MakeVertexBuffer(directContext,
                                                         meshVertices.data(),
                                                         meshVertices.size() * sizeof(MeshVertex));
    const auto indexBuffer = SkMeshes::MakeIndexBuffer(directContext, indexes.data(), indexes.size() * sizeof(std::uint16_t));
    if (!vertexBuffer || !indexBuffer) {
        return;
    }

    const auto canvasSize = canvas->getBaseLayerSize();
    const float viewport[2] = {static_cast<float>(canvasSize.width()), static_cast<float>(canvasSize.height())};

    auto uniforms = SkData::MakeUninitialized(specification->uniformSize());
    std::memset(uniforms->writable_data(), 0, uniforms->size());
    writeUniform(uniforms, *specification, "u_matrix", matrix.data(), matrix.size() * sizeof(float));
    writeUniform(uniforms, *specification, "u_viewport", viewport, sizeof(viewport));

    std::array<SkMesh::ChildPtr, 1> children;
    std::size_t childCount = 0;
    if (lineGradientDrawable) {
        auto& texture = static_cast<Texture2D&>(*lineImageTexture);
        if (texture.needsUpload()) {
            texture.upload();
        }
        const auto& image = texture.getImage();
        if (!image) {
            return;
        }
        auto gradientShader = image->makeShader(SkTileMode::kClamp,
                                                SkTileMode::kClamp,
                                                SkSamplingOptions(SkFilterMode::kLinear));
        if (!gradientShader) {
            return;
        }
        children[0] = std::move(gradientShader);
        childCount = 1;
    }

    SkPaint paint;
    paint.setAntiAlias(true);

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
