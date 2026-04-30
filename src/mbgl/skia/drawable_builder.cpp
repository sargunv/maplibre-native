#include "skia_impl.hpp"

#include <mbgl/gfx/drawable_builder_impl.hpp>

#include <cassert>
#include <cstring>

namespace mbgl {
namespace skia {

DrawableBuilder::DrawableBuilder(std::string name)
    : gfx::DrawableBuilder(std::move(name)) {}

std::unique_ptr<gfx::Drawable::DrawSegment> DrawableBuilder::createSegment(gfx::DrawMode mode, SegmentBase&& segment) {
    return std::make_unique<gfx::Drawable::DrawSegment>(mode, std::move(segment));
}

gfx::UniqueDrawable DrawableBuilder::createDrawable() const {
    return std::make_unique<Drawable>(drawableName.empty() ? name : drawableName);
}

void DrawableBuilder::init() {
    auto& drawable = static_cast<Drawable&>(*currentDrawable);

    drawable.setVertexAttrId(vertexAttrId);

    if (impl->rawVerticesCount && !impl->rawVertices.empty()) {
        auto raw = std::move(impl->rawVertices);
        drawable.setVertices(std::move(raw), impl->rawVerticesCount, impl->rawVerticesType);
    } else if (!impl->vertices.empty()) {
        const auto& verts = impl->vertices.vector();
        constexpr auto vertSize = sizeof(std::remove_reference<decltype(verts)>::type::value_type);
        std::vector<uint8_t> raw(verts.size() * vertSize);
        std::memcpy(raw.data(), verts.data(), raw.size());
        drawable.setVertices(std::move(raw), verts.size(), gfx::AttributeDataType::Short2);
    } else if (impl->rawVerticesCount) {
        drawable.setVertices({}, impl->rawVerticesCount, impl->rawVerticesType);
    }

    if (!impl->sharedIndexes && !impl->buildIndexes.empty()) {
        impl->sharedIndexes = std::make_shared<gfx::IndexVectorBase>(std::move(impl->buildIndexes));
    }
    assert(impl->sharedIndexes && impl->sharedIndexes->elements());
    if (impl->sharedIndexes && impl->sharedIndexes->elements()) {
        drawable.setIndexData(std::move(impl->sharedIndexes), std::move(impl->segments));
    }

    impl->clear();
    textures.fill(nullptr);
}

} // namespace skia
} // namespace mbgl
