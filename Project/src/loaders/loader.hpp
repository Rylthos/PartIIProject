#pragma once

#include "../voxel.hpp"

#include <optional>

class Loader {
  public:
    Loader(glm::uvec3 dimensions) : p_Dimensions(dimensions) { }
    virtual ~Loader() { }

    virtual std::optional<Voxel> getVoxel(glm::uvec3 index) = 0;

    glm::uvec3 getDimensions() { return p_Dimensions; }

  protected:
    glm::uvec3 p_Dimensions;
};
