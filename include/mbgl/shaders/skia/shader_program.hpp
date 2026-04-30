#pragma once

#include <mbgl/shaders/shader_program_base.hpp>

namespace mbgl {
namespace skia {

class ShaderProgram final : public gfx::ShaderProgramBase {
public:
    static constexpr auto Name = std::string_view("SkiaShaderProgram");

    explicit ShaderProgram(std::string name_);
    ~ShaderProgram() noexcept override;

    const std::string_view typeName() const noexcept override { return Name; }
    std::optional<size_t> getSamplerLocation(const size_t) const override;
    const gfx::VertexAttributeArray& getVertexAttributes() const override;
    const gfx::VertexAttributeArray& getInstanceAttributes() const override;

    const std::string& getName() const { return name; }

private:
    std::string name;
    gfx::VertexAttributeArray vertexAttributes;
    gfx::VertexAttributeArray instanceAttributes;
};

} // namespace skia
} // namespace mbgl
