#include <mbgl/gfx/headless_backend.hpp>
#include <mbgl/skia/renderer_backend.hpp>

namespace mbgl {
namespace skia {

class HeadlessBackend final : public gfx::HeadlessBackend {
public:
    HeadlessBackend(Size size_, gfx::HeadlessBackend::SwapBehaviour swapBehavior_, gfx::ContextMode contextMode_)
        : gfx::HeadlessBackend(size_),
          rendererBackend(size_, contextMode_),
          swapBehavior(swapBehavior_) {}

    PremultipliedImage readStillImage() override { return rendererBackend.readStillImage(); }

    gfx::RendererBackend* getRendererBackend() override { return &rendererBackend; }

    gfx::HeadlessBackend::SwapBehaviour getSwapBehavior() const { return swapBehavior; }

private:
    RendererBackend rendererBackend;
    gfx::HeadlessBackend::SwapBehaviour swapBehavior;
};

} // namespace skia

namespace gfx {

template <>
std::unique_ptr<gfx::HeadlessBackend> Backend::Create<gfx::Backend::Type::Skia>(
    const Size size, gfx::HeadlessBackend::SwapBehaviour swapBehavior, const gfx::ContextMode contextMode) {
    return std::make_unique<skia::HeadlessBackend>(size, swapBehavior, contextMode);
}

} // namespace gfx
} // namespace mbgl
