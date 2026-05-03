#pragma once

#include <mbgl/gfx/renderable.hpp>
#include <mbgl/gfx/renderer_backend.hpp>
#include <mbgl/util/image.hpp>

#include <include/core/SkRefCnt.h>
#include <include/gpu/ganesh/GrDirectContext.h>

#include <memory>

namespace mbgl {
namespace skia {

class MetalGpuHandles;

class Renderable final : public gfx::Renderable {
public:
    explicit Renderable(Size size_, GrDirectContext* directContext = nullptr);
    Renderable(Size size_,
               GrDirectContext* directContext,
               void* metalLayer,
               void* metalQueue);
    void setSize(Size size_, GrDirectContext* directContext = nullptr);
    void setSizeForLayer(Size size_,
                         GrDirectContext* directContext,
                         void* metalLayer,
                         void* metalQueue);
    PremultipliedImage readStillImage() const;
};

class RendererBackend final : public gfx::RendererBackend {
public:
    explicit RendererBackend(Size size = {1, 1}, gfx::ContextMode contextMode = gfx::ContextMode::Unique);
    ~RendererBackend() override;

    gfx::Renderable& getDefaultRenderable() override;
    void setSize(Size size_);
    void initShaders(gfx::ShaderRegistry&, const ProgramParameters&) override;
    GrDirectContext* getDirectContext() const;

    // Returns a bridged Metal device pointer (id<MTLDevice>) or nullptr when
    // the backend is not Metal-backed. Lifetime is owned by the backend.
    void* getMetalDevice() const noexcept;

    // Returns a bridged Metal command queue pointer (id<MTLCommandQueue>) or
    // nullptr when the backend is not Metal-backed. Lifetime is owned by the
    // backend.
    void* getMetalCommandQueue() const noexcept;

    // Switch the default renderable to draw into a CAMetalLayer the caller
    // attaches to its window's contentView. Pass a bridged CAMetalLayer*.
    // Set to nullptr to revert to the backend-owned offscreen surface.
    void attachMetalLayer(void* metalLayer);

    PremultipliedImage readStillImage() const;

protected:
    std::unique_ptr<gfx::Context> createContext() override;
    void activate() override;
    void deactivate() override;

private:
    sk_sp<GrDirectContext> directContext;
    std::unique_ptr<MetalGpuHandles> metalHandles;
    void* metalLayer = nullptr;
    Renderable defaultRenderable;
    Size lastSize;
};

} // namespace skia
} // namespace mbgl
