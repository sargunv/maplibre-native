#include "skia_impl.hpp"

#include <mbgl/renderer/paint_parameters.hpp>
#include <mbgl/shaders/background_layer_ubo.hpp>
#include <mbgl/shaders/fill_layer_ubo.hpp>
#include <mbgl/shaders/shader_defines.hpp>
#include <mbgl/util/mat4.hpp>

#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

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

Point<float> project(const std::array<float, 16>& matrix, const float x, const float y, const SkISize& size) {
    const mat4 m = {matrix[0],  matrix[1],  matrix[2],  matrix[3],
                    matrix[4],  matrix[5],  matrix[6],  matrix[7],
                    matrix[8],  matrix[9],  matrix[10], matrix[11],
                    matrix[12], matrix[13], matrix[14], matrix[15]};
    vec4 out;
    matrix::transformMat4(out, vec4{x, y, 0.0, 1.0}, m);

    const auto w = out[3] == 0.0 ? 1.0 : out[3];
    const auto ndcX = out[0] / w;
    const auto ndcY = out[1] / w;
    return {static_cast<float>((ndcX * 0.5 + 0.5) * size.width()),
            static_cast<float>((0.5 - ndcY * 0.5) * size.height())};
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
        const auto& attr = attrs->get(positionAttributeId) ? attrs->get(positionAttributeId) : attrs->get(shaders::idFillPosVertexAttribute);
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

    SkPaint paint;
    paint.setColor4f(color, nullptr);
    paint.setAntiAlias(true);

    const auto canvasSize = canvas->getBaseLayerSize();
    const auto& indexes = sharedIndexes->vector();
    for (const auto& segment : segments) {
        if (!segment || segment->getMode().type != gfx::DrawModeType::Triangles) {
            continue;
        }
        const auto& seg = segment->getSegment();
        const auto end = std::min(seg.indexOffset + seg.indexLength, indexes.size());
        for (auto i = seg.indexOffset; i + 2 < end; i += 3) {
            float x0, y0, x1, y1, x2, y2;
            if (!vertexReader.read(indexes[i], x0, y0) || !vertexReader.read(indexes[i + 1], x1, y1) ||
                !vertexReader.read(indexes[i + 2], x2, y2)) {
                continue;
            }
            const auto p0 = project(matrix, x0, y0, canvasSize);
            const auto p1 = project(matrix, x1, y1, canvasSize);
            const auto p2 = project(matrix, x2, y2, canvasSize);

            SkPathBuilder path;
            path.moveTo(p0.x, p0.y);
            path.lineTo(p1.x, p1.y);
            path.lineTo(p2.x, p2.y);
            path.close();
            canvas->drawPath(path.detach(), paint);
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
