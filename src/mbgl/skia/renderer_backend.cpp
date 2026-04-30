#include <mbgl/skia/renderer_backend.hpp>

#include <mbgl/skia/context.hpp>
#include <mbgl/shaders/program_parameters.hpp>
#include "skia_impl.hpp"

namespace mbgl {
namespace skia {

Renderable::Renderable(Size size_)
    : gfx::Renderable(size_, std::make_unique<RenderableResource>()) {}

RendererBackend::RendererBackend(Size size, gfx::ContextMode contextMode)
    : gfx::RendererBackend(contextMode),
      defaultRenderable(size) {}

RendererBackend::~RendererBackend() = default;

gfx::Renderable& RendererBackend::getDefaultRenderable() {
    return defaultRenderable;
}

void RendererBackend::initShaders(gfx::ShaderRegistry&, const ProgramParameters&) {}

std::unique_ptr<gfx::Context> RendererBackend::createContext() {
    return std::make_unique<Context>(*this);
}

void RendererBackend::activate() {}

void RendererBackend::deactivate() {}

} // namespace skia
} // namespace mbgl
