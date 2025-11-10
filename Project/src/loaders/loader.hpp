#pragma once

#include "../logger.hpp"
#include <glm/gtx/string_cast.hpp>

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

    virtual std::optional<Voxel> getVoxelMorton2(uint64_t mortonCode)
    {
        glm::uvec3 index = MortonCode::decode2(mortonCode);
        return getVoxel(index);
    }

    glm::uvec3 getDimensions() const { return p_Dimensions; }

  protected:
    glm::uvec3 p_Dimensions;
};
