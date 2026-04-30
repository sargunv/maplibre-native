#include <mbgl/skia/renderer_backend.hpp>

#include <mbgl/gfx/shader_registry.hpp>
#include <mbgl/skia/context.hpp>
#include <mbgl/shaders/skia/shader_group.hpp>
#include <mbgl/shaders/program_parameters.hpp>
#include "skia_impl.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace mbgl {
namespace skia {

Renderable::Renderable(Size size_, GrDirectContext* directContext)
    : gfx::Renderable(size_, std::make_unique<RenderableResource>(size_, directContext)) {}

RendererBackend::RendererBackend(Size size, gfx::ContextMode contextMode)
    : gfx::RendererBackend(contextMode),
      directContext(makeDefaultGaneshContext()),
      defaultRenderable(size, directContext.get()) {}

RendererBackend::~RendererBackend() = default;

gfx::Renderable& RendererBackend::getDefaultRenderable() {
    return defaultRenderable;
}

GrDirectContext* RendererBackend::getDirectContext() const {
    return directContext.get();
}

namespace {

void registerShaderGroup(gfx::ShaderRegistry& registry, const std::string& name) {
    using namespace std::string_literals;
    auto group = std::make_shared<ShaderGroup>(name);
    if (!registry.registerShaderGroup(std::move(group), name)) {
        assert(!"duplicate shader group");
        throw std::runtime_error("Failed to register "s + name + " with shader registry!");
    }
}

} // namespace

void RendererBackend::initShaders(gfx::ShaderRegistry& shaders, const ProgramParameters&) {
    registerShaderGroup(shaders, "BackgroundShader");
    registerShaderGroup(shaders, "BackgroundPatternShader");
    registerShaderGroup(shaders, "CircleShader");
    registerShaderGroup(shaders, "ClippingMaskProgram");
    registerShaderGroup(shaders, "CollisionBoxShader");
    registerShaderGroup(shaders, "CollisionCircleShader");
    registerShaderGroup(shaders, "CustomGeometryShader");
    registerShaderGroup(shaders, "CustomSymbolIconShader");
    registerShaderGroup(shaders, "DebugShader");
    registerShaderGroup(shaders, "FillShader");
    registerShaderGroup(shaders, "FillOutlineShader");
    registerShaderGroup(shaders, "FillPatternShader");
    registerShaderGroup(shaders, "FillOutlinePatternShader");
    registerShaderGroup(shaders, "FillOutlineTriangulatedShader");
    registerShaderGroup(shaders, "FillExtrusionShader");
    registerShaderGroup(shaders, "FillExtrusionPatternShader");
    registerShaderGroup(shaders, "HeatmapShader");
    registerShaderGroup(shaders, "HeatmapTextureShader");
    registerShaderGroup(shaders, "HillshadeShader");
    registerShaderGroup(shaders, "HillshadePrepareShader");
    registerShaderGroup(shaders, "ColorReliefShader");
    registerShaderGroup(shaders, "LineShader");
    registerShaderGroup(shaders, "LineGradientShader");
    registerShaderGroup(shaders, "LineSDFShader");
    registerShaderGroup(shaders, "LinePatternShader");
    registerShaderGroup(shaders, "LocationIndicatorShader");
    registerShaderGroup(shaders, "LocationIndicatorTexturedShader");
    registerShaderGroup(shaders, "RasterShader");
    registerShaderGroup(shaders, "SymbolIconShader");
    registerShaderGroup(shaders, "SymbolSDFShader");
    registerShaderGroup(shaders, "SymbolTextAndIconShader");
    registerShaderGroup(shaders, "WideVectorShader");
}

std::unique_ptr<gfx::Context> RendererBackend::createContext() {
    return std::make_unique<Context>(*this);
}

void RendererBackend::activate() {}

void RendererBackend::deactivate() {}

} // namespace skia
} // namespace mbgl
