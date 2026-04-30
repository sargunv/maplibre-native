#include "skia_impl.hpp"

#include <mbgl/gfx/drawable_tweaker.hpp>
#include <mbgl/renderer/paint_parameters.hpp>

namespace mbgl {
namespace skia {

TileLayerGroup::TileLayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name)
    : mbgl::TileLayerGroup(layerIndex, initialCapacity, std::move(name)),
      uniformBuffers(std::make_unique<UniformBufferArray>()) {}

void TileLayerGroup::render(RenderOrchestrator&, PaintParameters& parameters) {
    if (!enabled || !getDrawableCount() || !parameters.renderPass) {
        return;
    }

    visitDrawables([&](gfx::Drawable& drawable) {
        if (!drawable.getEnabled() || !drawable.hasRenderPass(parameters.pass)) {
            return;
        }

        for (const auto& tweaker : drawable.getTweakers()) {
            tweaker->execute(drawable, parameters);
        }

        static_cast<Drawable&>(drawable).draw(parameters, uniformBuffers.get());
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
    if (!enabled || !getDrawableCount() || !parameters.renderPass) {
        return;
    }

    visitDrawables([&](gfx::Drawable& drawable) {
        if (!drawable.getEnabled() || !drawable.hasRenderPass(parameters.pass)) {
            return;
        }

        for (const auto& tweaker : drawable.getTweakers()) {
            tweaker->execute(drawable, parameters);
        }

        static_cast<Drawable&>(drawable).draw(parameters, uniformBuffers.get());
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
