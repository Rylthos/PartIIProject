#pragma once

#include "../logger.hpp"
#include <glm/gtx/string_cast.hpp>

#include "../voxel.hpp"

#include "../morton_code.hpp"
#include "glm/exponential.hpp"
#include "glm/gtc/quaternion.hpp"

#include <optional>

class Loader {
  public:
    Loader(glm::uvec3 dimensions) : p_Dimensions(dimensions) { }
    virtual ~Loader() { }

    virtual std::optional<Voxel> getVoxel(glm::uvec3 index) = 0;
    virtual std::optional<Voxel> getVoxelMorton(uint64_t mortonCode)
    {
        glm::uvec3 index = MortonCode::decode(mortonCode);
        if (glm::any(glm::greaterThanEqual(index, p_Dimensions))) {
            return {};
        }

        return getVoxel(index);
    }

    virtual std::optional<Voxel> getVoxelMorton2(uint64_t mortonCode)
    {
        glm::uvec3 index = MortonCode::decode2(mortonCode);
        if (glm::any(glm::greaterThanEqual(index, p_Dimensions))) {
            return {};
        }

        return getVoxel(index);
    }

    glm::uvec3 getDimensions() const { return p_Dimensions; }

    glm::uvec3 getDimensionsDiv2() const { return getDimensionsDivN(2); }
    glm::uvec3 getDimensionsDiv4() const { return getDimensionsDivN(4); }
    glm::uvec3 getDimensionsDiv8() const { return getDimensionsDivN(8); }

    glm::uvec3 getDimensionsDivN(uint32_t n) const
    {
        assert(p_Dimensions.x != 1 && p_Dimensions.y != 1 && p_Dimensions.z != 1
            && "Breaks when only single voxel");

        glm::vec3 dim = p_Dimensions;

        dim = glm::log(dim) / glm::log(glm::vec3(n));

        return glm::uvec3(glm::pow(glm::vec3(n), glm::ceil(dim)));
    }

  protected:
    glm::uvec3 p_Dimensions;
};
