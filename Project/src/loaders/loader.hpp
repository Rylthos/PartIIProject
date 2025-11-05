#pragma once

#include "../voxel.hpp"

#include "../morton_code.hpp"

#include <optional>

class Loader {
  public:
    Loader(glm::uvec3 dimensions) : p_Dimensions(dimensions) { }
    virtual ~Loader() { }

    virtual std::optional<Voxel> getVoxel(glm::uvec3 index) = 0;
    virtual std::optional<Voxel> getVoxelMorton(uint64_t mortonCode)
    {
        glm::uvec3 index = MortonCode::decode(mortonCode);
        return getVoxel(index);
    }

    glm::uvec3 getDimensions() { return p_Dimensions; }

  protected:
    glm::uvec3 p_Dimensions;
};
