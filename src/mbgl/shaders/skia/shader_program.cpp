#include <mbgl/shaders/skia/shader_group.hpp>

#include <mbgl/util/hash.hpp>

namespace mbgl {
namespace skia {

ShaderProgram::ShaderProgram(std::string name_)
    : name(std::move(name_)) {}

ShaderProgram::~ShaderProgram() noexcept = default;

std::optional<size_t> ShaderProgram::getSamplerLocation(const size_t) const {
    return std::nullopt;
}

const gfx::VertexAttributeArray& ShaderProgram::getVertexAttributes() const {
    return vertexAttributes;
}

const gfx::VertexAttributeArray& ShaderProgram::getInstanceAttributes() const {
    return instanceAttributes;
}

ShaderGroup::ShaderGroup(std::string name_)
    : name(std::move(name_)) {}

gfx::ShaderPtr ShaderGroup::getOrCreateShader(gfx::Context&,
                                              const StringIDSetsPair& propertiesAsUniforms,
                                              std::string_view) {
    std::size_t seed = 0;
    mbgl::util::hash_combine(seed, propertyHash(propertiesAsUniforms));

    const auto shaderName = getShaderName(name, seed);
    if (auto shader = get<ShaderProgram>(shaderName)) {
        return shader;
    }

    auto shader = std::make_shared<ShaderProgram>(shaderName);
    if (!registerShader(shader, shaderName)) {
        return nullptr;
    }
    return shader;
}

} // namespace skia
} // namespace mbgl
