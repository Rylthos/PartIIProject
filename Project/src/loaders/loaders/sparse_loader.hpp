#pragma once

#include "loader.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <optional>
#include <unordered_map>

class SparseLoader : public Loader {

  public:
    SparseLoader(glm::uvec3 dimensions, std::unordered_map<glm::ivec3, glm::vec3> voxels);

    ~SparseLoader() { }

    std::optional<glm::vec3> getVoxel(glm::uvec3 index) override;

  private:
    std::unordered_map<glm::ivec3, glm::vec3> m_Voxels;
};
