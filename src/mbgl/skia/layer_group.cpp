#include "skia_impl.hpp"

namespace mbgl {
namespace skia {

TileLayerGroup::TileLayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name)
    : mbgl::TileLayerGroup(layerIndex, initialCapacity, std::move(name)),
      uniformBuffers(std::make_unique<UniformBufferArray>()) {}

void TileLayerGroup::render(RenderOrchestrator&, PaintParameters& parameters) {
    visitDrawables([&](gfx::Drawable& drawable) {
        if (drawable.getEnabled()) {
            drawable.draw(parameters);
        }
    });
}

const gfx::UniformBufferArray& TileLayerGroup::getUniformBuffers() const {
    return *uniformBuffers;
}

gfx::UniformBufferArray& TileLayerGroup::mutableUniformBuffers() {
    return *uniformBuffers;
}

LayerGroup::LayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name)
    : mbgl::LayerGroup(layerIndex, initialCapacity, std::move(name)),
      uniformBuffers(std::make_unique<UniformBufferArray>()) {}

void LayerGroup::render(RenderOrchestrator&, PaintParameters& parameters) {
    visitDrawables([&](gfx::Drawable& drawable) {
        if (drawable.getEnabled()) {
            drawable.draw(parameters);
        }
    });
}

const gfx::UniformBufferArray& LayerGroup::getUniformBuffers() const {
    return *uniformBuffers;
}

gfx::UniformBufferArray& LayerGroup::mutableUniformBuffers() {
    return *uniformBuffers;
}

} // namespace skia
} // namespace mbgl
