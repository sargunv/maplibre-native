#include "skia_impl.hpp"

#include <mbgl/renderer/paint_parameters.hpp>
#include <mbgl/skia/renderer_backend.hpp>
#include <mbgl/shaders/background_layer_ubo.hpp>
#include <mbgl/shaders/fill_layer_ubo.hpp>
#include <mbgl/shaders/shader_defines.hpp>

#include <include/core/SkBlender.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <include/core/SkMesh.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkString.h>
#include <include/gpu/ganesh/SkMeshGanesh.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
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

struct MeshVertex {
    float position[2];
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
    return SkColor4f{color.r, color.g, color.b, color.a * opacity};
}

std::array<float, 16> identityMatrix() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

sk_sp<SkMeshSpecification> solidColorMeshSpecification() {
    static sk_sp<SkMeshSpecification> specification;
    if (specification) {
        return specification;
    }

    using Attribute = SkMeshSpecification::Attribute;
    const Attribute attributes[] = {{Attribute::Type::kFloat2, 0, SkString("a_pos")}};

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
            return varyings;
        }
    )");

    const SkString fragmentShader(R"(
        layout(color) uniform half4 u_color;

        float2 main(const Varyings varyings, out half4 color) {
            color = u_color;
            return varyings.position;
        }
    )");

    auto [spec, error] = SkMeshSpecification::Make(attributes,
                                                   sizeof(MeshVertex),
                                                   SkSpan<const SkMeshSpecification::Varying>(),
                                                   vertexShader,
                                                   fragmentShader);
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

    if (const auto* drawableUBO = getUBO<shaders::FillDrawableUBO>(&getUniformBuffers(), shaders::idFillDrawableUBO)) {
        matrix = drawableUBO->matrix;
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
        const auto& attr = attrs->get(positionAttributeId) ? attrs->get(positionAttributeId)
                                                          : attrs->get(shaders::idFillPosVertexAttribute);
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

    const auto specification = solidColorMeshSpecification();
    if (!specification) {
        return;
    }

    if (vertexReader.count > std::numeric_limits<std::uint16_t>::max()) {
        return;
    }

    std::vector<MeshVertex> meshVertices(vertexReader.count);
    for (std::size_t i = 0; i < vertexReader.count; ++i) {
        if (!vertexReader.read(static_cast<std::uint16_t>(i), meshVertices[i].position[0], meshVertices[i].position[1])) {
            return;
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
    const float uniformColor[4] = {color.fR, color.fG, color.fB, color.fA};

    auto uniforms = SkData::MakeUninitialized(specification->uniformSize());
    std::memset(uniforms->writable_data(), 0, uniforms->size());
    writeUniform(uniforms, *specification, "u_matrix", matrix.data(), matrix.size() * sizeof(float));
    writeUniform(uniforms, *specification, "u_viewport", viewport, sizeof(viewport));
    writeUniform(uniforms, *specification, "u_color", uniformColor, sizeof(uniformColor));

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

        const auto mesh = SkMesh::MakeIndexed(specification,
                                              SkMesh::Mode::kTriangles,
                                              vertexBuffer,
                                              meshVertices.size(),
                                              /*vertexOffset=*/0,
                                              indexBuffer,
                                              end - seg.indexOffset,
                                              seg.indexOffset * sizeof(std::uint16_t),
                                              uniforms,
                                              SkSpan<SkMesh::ChildPtr>(),
                                              SkRect::MakeWH(viewport[0], viewport[1]));
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
