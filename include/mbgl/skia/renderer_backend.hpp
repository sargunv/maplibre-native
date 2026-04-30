#pragma once

#include <mbgl/gfx/renderable.hpp>
#include <mbgl/gfx/renderer_backend.hpp>

namespace mbgl {
namespace skia {

class Renderable final : public gfx::Renderable {
public:
    explicit Renderable(Size size_);
};

class RendererBackend final : public gfx::RendererBackend {
public:
    explicit RendererBackend(Size size = {1, 1}, gfx::ContextMode contextMode = gfx::ContextMode::Unique);
    ~RendererBackend() override;

    gfx::Renderable& getDefaultRenderable() override;
    void initShaders(gfx::ShaderRegistry&, const ProgramParameters&) override;

protected:
    std::unique_ptr<gfx::Context> createContext() override;
    void activate() override;
    void deactivate() override;

private:
    Renderable defaultRenderable;
};

} // namespace skia
} // namespace mbgl
