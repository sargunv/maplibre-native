#pragma once

#include <mbgl/gfx/shader_group.hpp>
#include <mbgl/shaders/skia/shader_program.hpp>

namespace mbgl {
namespace skia {

class ShaderGroup final : public gfx::ShaderGroup {
public:
    explicit ShaderGroup(std::string name_);

    gfx::ShaderPtr getOrCreateShader(gfx::Context&,
                                     const StringIDSetsPair& propertiesAsUniforms,
                                     std::string_view firstAttribName = "a_pos") override;

private:
    std::string name;
};

} // namespace skia
} // namespace mbgl
