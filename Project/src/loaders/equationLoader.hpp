#pragma once

#include <optional>

#include "../voxel.hpp"

#include "loader.hpp"

class EquationLoader : public Loader {
    using FunctionType = std::function<std::optional<Voxel>(glm::uvec3, glm::uvec3)>;

  public:
    EquationLoader(glm::uvec3 dimensions, FunctionType function);

    ~EquationLoader() { }

    std::optional<Voxel> getVoxel(glm::uvec3 index) override;

  private:
    FunctionType m_Function;
};
