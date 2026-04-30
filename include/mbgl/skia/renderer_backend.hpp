#pragma once

#include <mbgl/gfx/renderable.hpp>
#include <mbgl/gfx/renderer_backend.hpp>
#include <mbgl/util/image.hpp>

#include <include/core/SkRefCnt.h>
#include <include/gpu/ganesh/GrDirectContext.h>

namespace mbgl {
namespace skia {

class Renderable final : public gfx::Renderable {
public:
    explicit Renderable(Size size_, GrDirectContext* directContext = nullptr);
    PremultipliedImage readStillImage() const;
};

class RendererBackend final : public gfx::RendererBackend {
public:
    explicit RendererBackend(Size size = {1, 1}, gfx::ContextMode contextMode = gfx::ContextMode::Unique);
    ~RendererBackend() override;

    gfx::Renderable& getDefaultRenderable() override;
    void initShaders(gfx::ShaderRegistry&, const ProgramParameters&) override;
    GrDirectContext* getDirectContext() const;
    PremultipliedImage readStillImage() const;

protected:
    std::unique_ptr<gfx::Context> createContext() override;
    void activate() override;
    void deactivate() override;

private:
    sk_sp<GrDirectContext> directContext;
    Renderable defaultRenderable;
};

} // namespace skia
} // namespace mbgl
