#pragma once

#include "loader.hpp"

#include <optional>

class EquationLoader : public Loader {
    using FunctionType = std::function<std::optional<glm::vec3>(glm::uvec3, glm::uvec3)>;

  public:
    EquationLoader(glm::uvec3 dimensions, FunctionType function);

    ~EquationLoader() { }

    std::optional<glm::vec3> getVoxel(glm::uvec3 index) override;

  private:
    FunctionType m_Function;
};
