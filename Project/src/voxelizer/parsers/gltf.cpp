#include "gltf.hpp"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include <glm/gtx/string_cast.hpp>
#include <unordered_map>

namespace ParserImpl {

void loadDiffuse(aiMaterial* material, aiTextureType type,
    std::unordered_map<int32_t, Material>& materials, int32_t matIndex)
{
    printf("Mat Index: %d | Texture Count: %d\n", matIndex, material->GetTextureCount(type));
    assert(
        material->GetTextureCount(type) <= 1 && "Multiple material to same matIndex not supported");

    if (material->GetTextureCount(type) == 0) {
        Material mat;
        aiColor3D colour;
        material->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
        mat.diffuse.r = colour.r;
        mat.diffuse.g = colour.g;
        mat.diffuse.b = colour.b;
        materials.insert({ matIndex, mat });

    } else {
        for (uint32_t i = 0; i < material->GetTextureCount(type); i++) {

            Material mat;

            aiString str;
            material->GetTexture(type, i, &str);

            if (!materials.contains(matIndex)) {
                parseImage(str.C_Str(), mat);

                materials.insert({ matIndex, mat });
            }
        }
    }
}

std::vector<Triangle> processMesh(
    aiMesh* mesh, const aiScene* scene, std::unordered_map<int32_t, Material>& materials)
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;

    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        glm::vec3 position = {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z,
        };

        positions.push_back(position);

        glm::vec2 uv(0.f);
        if (mesh->mTextureCoords[0]) {
            uv = {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y,
            };
        }

        uvs.push_back(uv);
    }

    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    int32_t matIndex = -1;
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        loadDiffuse(material, aiTextureType_DIFFUSE, materials, mesh->mMaterialIndex);
        matIndex = mesh->mMaterialIndex;
    }

    std::vector<Triangle> triangles;
    for (uint32_t i = 0; i < indices.size() / 3; i++) {
        uint32_t index = i * 3;

        Triangle triangle;
        triangle.positions[0] = positions[indices[index + 0]];
        triangle.positions[1] = positions[indices[index + 1]];
        triangle.positions[2] = positions[indices[index + 2]];

        triangle.texture[0] = glm::vec3(uvs[indices[index + 0]], 0.f);
        triangle.texture[1] = glm::vec3(uvs[indices[index + 1]], 0.f);
        triangle.texture[2] = glm::vec3(uvs[indices[index + 2]], 0.f);

        triangle.matIndex = matIndex;

        triangles.push_back(triangle);
    }

    return triangles;
}

std::vector<Triangle> parseAssimpNode(
    aiNode* node, const aiScene* scene, std::unordered_map<int32_t, Material>& materials)
{
    std::vector<Triangle> triangles;

    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        auto newTriangles = processMesh(mesh, scene, materials);

        triangles.insert(triangles.end(), newTriangles.begin(), newTriangles.end());
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        auto newTriangles = parseAssimpNode(node->mChildren[i], scene, materials);

        triangles.insert(triangles.end(), newTriangles.begin(), newTriangles.end());
    }

    return triangles;
}

ParserRet parseGltf(std::filesystem::path path, const ParserArgs& args)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path.string().c_str(), aiProcess_Triangulate);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode) {
        fprintf(
            stderr, "Failed to load %s | %s\n", path.string().c_str(), importer.GetErrorString());
        exit(-1);
    }

    std::unordered_map<int32_t, Material> materials;

    auto triangles = parseAssimpNode(scene->mRootNode, scene, materials);

    for (auto& triangle : triangles) {
        printf("%s | %s | %s\n", glm::to_string(triangle.positions[0]).c_str(),
            glm::to_string(triangle.positions[1]).c_str(),
            glm::to_string(triangle.positions[2]).c_str());
    }
    printf("Triangles: %ld\n", triangles.size());

    return parseMesh(triangles, materials, args);
}

}
