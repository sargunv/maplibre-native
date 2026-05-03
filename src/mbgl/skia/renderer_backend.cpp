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

Renderable::Renderable(Size size_, GrDirectContext* directContext, void* metalLayer, void* metalQueue)
    : gfx::Renderable(size_,
                      std::make_unique<RenderableResource>(size_, directContext, metalLayer, metalQueue)) {}

void Renderable::setSize(Size size_, GrDirectContext* directContext) {
    size = size_;
    setResource(std::make_unique<RenderableResource>(size_, directContext));
}

void Renderable::setSizeForLayer(Size size_,
                                 GrDirectContext* directContext,
                                 void* metalLayer,
                                 void* metalQueue) {
    size = size_;
    setResource(std::make_unique<RenderableResource>(size_, directContext, metalLayer, metalQueue));
}

PremultipliedImage Renderable::readStillImage() const {
    PremultipliedImage image(size);
    const auto& resource = getResource<RenderableResource>();
    const auto info = SkImageInfo::Make(size.width, size.height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    if (resource.getSurface() && image.valid()) {
        resource.getSurface()->readPixels(info, image.data.get(), image.stride(), 0, 0);
    }
    return image;
}

namespace {

GaneshGpuContext setupGaneshContext() {
    return makeDefaultGaneshContext();
}

} // namespace

RendererBackend::RendererBackend(Size size, gfx::ContextMode contextMode)
    : gfx::RendererBackend(contextMode),
      directContext(),
      metalHandles(),
      defaultRenderable(size, nullptr),
      lastSize(size) {
    auto ganesh = setupGaneshContext();
    directContext = std::move(ganesh.context);
    metalHandles = std::make_unique<MetalGpuHandles>(std::move(ganesh.metal));
    defaultRenderable.setSize(size, directContext.get());
}

RendererBackend::~RendererBackend() = default;

gfx::Renderable& RendererBackend::getDefaultRenderable() {
    return defaultRenderable;
}

void RendererBackend::setSize(Size size_) {
    lastSize = size_;
    if (metalLayer) {
        updateMetalLayerDrawableSize(metalLayer, size_);
        defaultRenderable.setSizeForLayer(
            size_, directContext.get(), metalLayer, metalHandles ? metalHandles->getQueue() : nullptr);
    } else {
        defaultRenderable.setSize(size_, directContext.get());
    }
}

GrDirectContext* RendererBackend::getDirectContext() const {
    return directContext.get();
}

void* RendererBackend::getMetalDevice() const noexcept {
    return metalHandles ? metalHandles->getDevice() : nullptr;
}

void* RendererBackend::getMetalCommandQueue() const noexcept {
    return metalHandles ? metalHandles->getQueue() : nullptr;
}

void RendererBackend::attachMetalLayer(void* layer) {
    metalLayer = layer;
    if (metalLayer) {
        updateMetalLayerDrawableSize(metalLayer, lastSize);
        defaultRenderable.setSizeForLayer(
            lastSize, directContext.get(), metalLayer, metalHandles ? metalHandles->getQueue() : nullptr);
    } else {
        defaultRenderable.setSize(lastSize, directContext.get());
    }
}

PremultipliedImage RendererBackend::readStillImage() const {
    return defaultRenderable.readStillImage();
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
