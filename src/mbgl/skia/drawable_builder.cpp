#include "skia_impl.hpp"

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

void DrawableBuilder::init() {}

} // namespace skia
} // namespace mbgl
