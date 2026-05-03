#include "skia_impl.hpp"

namespace mbgl {
namespace skia {

MetalGpuHandles::MetalGpuHandles(void*, void*) noexcept {}
MetalGpuHandles::~MetalGpuHandles() = default;
MetalGpuHandles::MetalGpuHandles(MetalGpuHandles&&) noexcept = default;
MetalGpuHandles& MetalGpuHandles::operator=(MetalGpuHandles&&) noexcept = default;

GaneshGpuContext makeDefaultGaneshContext() {
    return {};
}

sk_sp<SkSurface> wrapMetalLayerSurface(GrDirectContext*, void*, void**) {
    return nullptr;
}

void presentMetalDrawable(void*, void*) {}

void releaseMetalDrawable(void*) {}

void updateMetalLayerDrawableSize(void*, Size) {}

} // namespace skia
} // namespace mbgl
