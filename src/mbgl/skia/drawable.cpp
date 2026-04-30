#include "skia_impl.hpp"

namespace mbgl {
namespace skia {

Drawable::Drawable(std::string name)
    : gfx::Drawable(std::move(name)),
      uniformBuffers(std::make_unique<UniformBufferArray>()) {}

void Drawable::draw(PaintParameters&) const {
    // SkMesh drawing will be wired here once shader specifications are ported.
}

void Drawable::updateVertexAttributes(gfx::VertexAttributeArrayPtr attributes,
                                      std::size_t,
                                      gfx::DrawMode,
                                      gfx::IndexVectorBasePtr indexes,
                                      const SegmentBase*,
                                      std::size_t) {
    setVertexAttributes(std::move(attributes));
    sharedIndexes = std::move(indexes);
}

void Drawable::setVertices(std::vector<std::uint8_t>&& vertices_, std::size_t, gfx::AttributeDataType) {
    vertices = std::move(vertices_);
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
