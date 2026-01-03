#include "assimp.hpp"

#include "assimp/Importer.hpp"
#include "assimp/anim.h"
#include "assimp/postprocess.h"
#include "assimp/quaternion.h"
#include "assimp/scene.h"
#include "general.hpp"
#include "glm/gtc/quaternion.hpp"

#include <glm/gtx/string_cast.hpp>
#include <unordered_map>
#include <vector>

namespace ParserImpl {

struct Channel {
    std::string target;

    std::vector<std::pair<double, glm::vec3>> positions;
    std::vector<std::pair<double, glm::quat>> rotations;
    std::vector<std::pair<double, glm::vec3>> scales;
};

struct Animation {
    std::string name;
    double duration;
    std::vector<Channel> channels;
};

struct Mesh {
    std::string name;
    std::vector<Triangle> triangles;
};

struct Node {
    std::string name;
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;
};

void loadDiffuse(std::filesystem::path basepath, aiMaterial* material, aiTextureType type,
    std::unordered_map<int32_t, Material>& materials, int32_t matIndex)
{
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

            aiColor3D colour;
            material->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
            mat.diffuse.r = colour.r;
            mat.diffuse.g = colour.g;
            mat.diffuse.b = colour.b;

            aiString str;
            material->GetTexture(type, i, &str);

            mat.validTexture = true;

            if (!materials.contains(matIndex)) {
                parseImage(basepath / str.C_Str(), mat);

                materials.insert({ matIndex, mat });
            }
        }
    }
}

Mesh processMesh(std::filesystem::path basepath, aiMesh* mesh, const aiScene* scene,
    aiMatrix4x4 transform, std::unordered_map<int32_t, Material>& materials)
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;

    transform = transform.Transpose();

    // clang-format off
    glm::mat4 glmTransform {
        transform.a1, transform.a2, transform.a3, transform.a4,
        transform.b1, transform.b2, transform.b3, transform.b4,
        transform.c1, transform.c2, transform.c3, transform.c4,
        transform.d1, transform.d2, transform.d3, transform.d4
    };
    // clang-format on

    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        glm::vec4 position = {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z,
            1.f,
        };

        positions.push_back(glmTransform * position * glm::vec4(1.f, -1.f, 1.f, 1.f));

        glm::vec2 uv(0.f);
        if (mesh->mTextureCoords[0]) {
            uv = {
                mesh->mTextureCoords[0][i].x,
                1.f - mesh->mTextureCoords[0][i].y,
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

        loadDiffuse(basepath, material, aiTextureType_DIFFUSE, materials, mesh->mMaterialIndex);
        matIndex = mesh->mMaterialIndex;
    }

    Mesh returnMesh;
    returnMesh.name = mesh->mName.C_Str();

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

        returnMesh.triangles.push_back(triangle);
    }

    return returnMesh;
}

Node parseAssimpNode(std::filesystem::path basepath, aiNode* node, const aiScene* scene,
    aiMatrix4x4 transform, std::unordered_map<int32_t, Material>& materials)
{
    Node returnNode;
    std::vector<Triangle> triangles;
    returnNode.name = node->mName.C_Str();

    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        Mesh newMesh = processMesh(basepath, mesh, scene, transform, materials);

        returnNode.meshes.push_back(newMesh);
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        aiMatrix4x4 childTransform = transform * node->mChildren[i]->mTransformation;
        Node newNode
            = parseAssimpNode(basepath, node->mChildren[i], scene, childTransform, materials);

        returnNode.nodes.push_back(newNode);
    }

    return returnNode;
}

std::vector<Animation> parseAnimations(const aiScene* scene)
{
    if (!scene->HasAnimations())
        return {};

    std::vector<Animation> animations;

    for (uint32_t i = 0; i < scene->mNumAnimations; i++) {
        aiAnimation* anim = scene->mAnimations[i];

        Animation animation;
        animation.name = anim->mName.C_Str();
        animation.duration = anim->mDuration;

        for (uint32_t j = 0; j < anim->mNumChannels; j++) {
            aiNodeAnim* nodeAnim = anim->mChannels[j];

            Channel channel;

            channel.target = nodeAnim->mNodeName.C_Str();

            for (uint32_t k = 0; k < nodeAnim->mNumPositionKeys; k++) {
                aiVectorKey pos = nodeAnim->mPositionKeys[k];

                channel.positions.push_back({
                    pos.mTime,
                    { pos.mValue.x, -pos.mValue.y, pos.mValue.z },
                });
            }

            for (uint32_t k = 0; k < nodeAnim->mNumRotationKeys; k++) {
                aiQuatKey quat = nodeAnim->mRotationKeys[k];

                channel.rotations.push_back({
                    quat.mTime,
                    { quat.mValue.x, quat.mValue.y, quat.mValue.z, quat.mValue.w },
                });
            }

            for (uint32_t k = 0; k < nodeAnim->mNumScalingKeys; k++) {
                aiVectorKey scale = nodeAnim->mScalingKeys[k];

                channel.scales.push_back({
                    scale.mTime,
                    { scale.mValue.x, scale.mValue.y, scale.mValue.z },
                });
            }

            animation.channels.push_back(channel);
        }

        animations.push_back(animation);
    }

    return animations;
}

glm::mat4 evaluateTransform(
    std::string node, const Animation& animation, float time, glm::mat4 transform)
{
    for (auto& channel : animation.channels) {
        if (channel.target != node)
            continue;

        if (channel.rotations.size() != 0) {
            for (size_t i = 0; i < channel.rotations.size() - 1; i++) {
                if (time >= channel.rotations[i].first && time < channel.rotations[i + 1].first) {
                    float t = (time - channel.rotations[i].first)
                        / (channel.rotations[i + 1].first - channel.rotations[i].first);

                    glm::quat lerp = glm::lerp(
                        channel.rotations[i].second, channel.rotations[i + 1].second, t);

                    transform = transform * glm::mat4_cast(lerp);
                }
            }
        }

        if (channel.scales.size() != 0) {
            for (size_t i = 0; i < channel.scales.size() - 1; i++) {
                if (time >= channel.scales[i].first && time < channel.scales[i + 1].first) {
                    float t = (time - channel.scales[i].first)
                        / (channel.scales[i + 1].first - channel.scales[i].first);

                    glm::vec3 lerp
                        = (1.f - t) * channel.scales[i].second + t * channel.scales[i + 1].second;

                    transform = glm::scale(transform, lerp);
                }
            }
        }

        if (channel.positions.size() != 0) {
            for (size_t i = 0; i < channel.positions.size() - 1; i++) {
                if (time >= channel.positions[i].first && time < channel.positions[i + 1].first) {
                    float t = (time - channel.positions[i].first)
                        / (channel.positions[i + 1].first - channel.positions[i].first);

                    glm::vec3 lerp = (1.f - t) * channel.positions[i].second
                        + t * channel.positions[i + 1].second;

                    transform = glm::translate(transform, lerp);
                }
            }
        }
    }

    return transform;
}

std::vector<Triangle> evaluateNodes(
    const Node& node, const Animation& animation, float time, glm::mat4 transform)
{
    std::vector<Triangle> triangles;

    transform = evaluateTransform(node.name, animation, time, transform);

    for (const auto& mesh : node.meshes) {
        for (size_t i = 0; i < mesh.triangles.size(); i++) {
            triangles.push_back(transformTriangle(mesh.triangles[i], transform));
        }
    }

    for (const auto& node : node.nodes) {
        auto nodeTriangles = evaluateNodes(node, animation, time, transform);
        triangles.insert(triangles.end(), nodeTriangles.begin(), nodeTriangles.end());
    }

    return triangles;
}

ParserRet parseAssimp(std::filesystem::path path, const ParserArgs& args)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(
        path.string().c_str(), aiProcess_Triangulate | aiProcess_ConvertToLeftHanded);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode) {
        fprintf(
            stderr, "Failed to load %s | %s\n", path.string().c_str(), importer.GetErrorString());
        exit(-1);
    }

    std::unordered_map<int32_t, Material> materials;

    std::vector<Animation> animations = parseAnimations(scene);
    Animation animation = animations[0];

    Node baseNode = parseAssimpNode(
        path.parent_path(), scene->mRootNode, scene, scene->mRootNode->mTransformation, materials);

    glm::uvec3 dimensions;
    std::vector<std::unordered_map<glm::ivec3, glm::vec3>> baseVoxels;

    std::vector<std::vector<Triangle>> meshes;

    for (uint32_t frame = 0; frame < args.frames; frame++) {
        float t = (frame / (float)args.frames) * animation.duration;

        auto frameTriangles = evaluateNodes(baseNode, animation, t, glm::mat4(1.f));

        meshes.push_back(frameTriangles);
    }

    std::tie(dimensions, baseVoxels) = parseMeshes(meshes, materials, args);

    return { dimensions, baseVoxels };
}

}
