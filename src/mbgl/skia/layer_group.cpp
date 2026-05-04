#include "skia_impl.hpp"

#include <mbgl/gfx/drawable_tweaker.hpp>
#include <mbgl/renderer/paint_parameters.hpp>
#include <mbgl/tile/tile_id.hpp>
#include <mbgl/util/constants.hpp>

#include <include/core/SkPath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace mbgl {
namespace skia {

namespace {

std::array<float, 2> projectToScreen(const mat4& matrix, const float viewport[2], const float x, const float y) {
    const auto clipX = matrix[0] * x + matrix[4] * y + matrix[12];
    const auto clipY = matrix[1] * x + matrix[5] * y + matrix[13];
    const auto clipW = matrix[3] * x + matrix[7] * y + matrix[15];
    const auto invW = clipW == 0.0 ? 1.0 : 1.0 / clipW;
    const auto ndcX = clipX * invW;
    const auto ndcY = clipY * invW;
    return {static_cast<float>((ndcX * 0.5 + 0.5) * viewport[0]), static_cast<float>((0.5 - ndcY * 0.5) * viewport[1])};
}

float projectedW(const mat4& matrix, const float x, const float y) {
    return static_cast<float>(matrix[3] * x + matrix[7] * y + matrix[15]);
}

bool isFinite(const std::array<float, 2>& point) {
    return std::isfinite(point[0]) && std::isfinite(point[1]);
}

std::optional<SkPath> tileClipPath(const mat4& matrix, const Size size) {
    const float viewport[2] = {static_cast<float>(size.width), static_cast<float>(size.height)};

    const auto topLeft = projectToScreen(matrix, viewport, 0.0f, 0.0f);
    const auto topRight = projectToScreen(matrix, viewport, static_cast<float>(util::EXTENT), 0.0f);
    const auto bottomRight = projectToScreen(
        matrix, viewport, static_cast<float>(util::EXTENT), static_cast<float>(util::EXTENT));
    const auto bottomLeft = projectToScreen(matrix, viewport, 0.0f, static_cast<float>(util::EXTENT));
    constexpr float projectedNearW = 1.0e-4f;
    if (projectedW(matrix, 0.0f, 0.0f) < projectedNearW ||
        projectedW(matrix, static_cast<float>(util::EXTENT), 0.0f) < projectedNearW ||
        projectedW(matrix, static_cast<float>(util::EXTENT), static_cast<float>(util::EXTENT)) < projectedNearW ||
        projectedW(matrix, 0.0f, static_cast<float>(util::EXTENT)) < projectedNearW) {
        return std::nullopt;
    }
    if (!isFinite(topLeft) || !isFinite(topRight) || !isFinite(bottomRight) || !isFinite(bottomLeft)) {
        return std::nullopt;
    }

    const std::array<SkPoint, 4> points = {SkPoint::Make(topLeft[0], topLeft[1]),
                                           SkPoint::Make(topRight[0], topRight[1]),
                                           SkPoint::Make(bottomRight[0], bottomRight[1]),
                                           SkPoint::Make(bottomLeft[0], bottomLeft[1])};
    return SkPath::Polygon(points, true);
}

std::optional<SkPath> tileClipPath(const PaintParameters& parameters,
                                   const UnwrappedTileID& tileID,
                                   const Size canvasSize) {
    // Build the clip in canvas (device) pixels so it matches the shader's
    // u_viewport. parameters.state.getSize() is logical and would shrink the
    // clip by 1/pixelRatio, masking out most of the canvas at pixelRatio>1.
    return tileClipPath(parameters.matrixForTile(tileID), canvasSize);
}

void drawWithTileClip(gfx::Drawable& drawable,
                      PaintParameters& parameters,
                      const gfx::UniformBufferArray* uniformBuffers) {
    auto& skiaDrawable = static_cast<Drawable&>(drawable);
    auto* renderPass = static_cast<RenderPass*>(parameters.renderPass.get());
    auto* canvas = renderPass ? renderPass->getCanvas() : nullptr;
    const auto& tileID = drawable.getTileID();

    if (canvas && drawable.getEnableStencil() && tileID) {
        const auto layerSize = canvas->getBaseLayerSize();
        const Size canvasSize{static_cast<uint32_t>(layerSize.width()),
                              static_cast<uint32_t>(layerSize.height())};
        const auto path = tileClipPath(parameters, tileID->toUnwrapped(), canvasSize);
        if (path) {
            SkAutoCanvasRestore autoRestore(canvas, true);
            canvas->clipPath(*path, SkClipOp::kIntersect, false);
            skiaDrawable.draw(parameters, uniformBuffers);
            return;
        }
    }

    skiaDrawable.draw(parameters, uniformBuffers);
}

} // namespace

bool clipCanvasToTileForTests(SkCanvas& canvas, const mat4& matrix, const Size size) {
    const auto path = tileClipPath(matrix, size);
    if (!path) {
        return false;
    }
    canvas.clipPath(*path, SkClipOp::kIntersect, false);
    return true;
}

TileLayerGroup::TileLayerGroup(int32_t layerIndex, std::size_t initialCapacity, std::string name)
    : mbgl::TileLayerGroup(layerIndex, initialCapacity, std::move(name)),
      uniformBuffers(std::make_unique<UniformBufferArray>()) {}

void TileLayerGroup::render(RenderOrchestrator&, PaintParameters& parameters) {
    if (!enabled || !getDrawableCount() || !parameters.renderPass) {
        return;
    }

    // Skia has no stencil buffer to mask painted areas, so without ordering
    // intervention coarser-zoom tiles can paint over finer ones when both are
    // present (overscan, multi-LOD selection, low-zoom views with wrap copies).
    // Render back-to-front by tile zoom: coarsest first, finest last, so the
    // higher-detail tile always wins the overlap. stable_sort preserves the
    // priority/ID order from sortedDrawables for ties.
    struct OrderedDrawable {
        gfx::Drawable* drawable = nullptr;
        std::int32_t z = 0;
    };
    std::vector<OrderedDrawable> ordered;
    ordered.reserve(getDrawableCount());

    visitDrawables([&](gfx::Drawable& drawable) {
        if (!drawable.getEnabled() || !drawable.hasRenderPass(parameters.pass)) {
            return;
        }
        OrderedDrawable entry;
        entry.drawable = &drawable;
        if (const auto& tileID = drawable.getTileID()) {
            entry.z = static_cast<std::int32_t>(tileID->overscaledZ);
        } else {
            entry.z = std::numeric_limits<std::int32_t>::min();
        }
        ordered.push_back(entry);
    });

    std::stable_sort(ordered.begin(), ordered.end(), [](const OrderedDrawable& a, const OrderedDrawable& b) {
        return a.z < b.z;
    });

    for (const auto& entry : ordered) {
        for (const auto& tweaker : entry.drawable->getTweakers()) {
            tweaker->execute(*entry.drawable, parameters);
        }
        drawWithTileClip(*entry.drawable, parameters, uniformBuffers.get());
    }
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
