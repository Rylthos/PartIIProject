#include "assimp.hpp"

#include "assimp/Importer.hpp"
#include "assimp/anim.h"
#include "assimp/matrix4x4.h"
#include "assimp/mesh.h"
#include "assimp/postprocess.h"
#include "assimp/quaternion.h"
#include "assimp/scene.h"
#include "general.hpp"
#include "glm/geometric.hpp"
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

struct Bone {
    std::string name;

    glm::mat4 offset;

    std::unordered_map<size_t, float> vertexWeights;
};

struct Node {
    std::string name;
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;

    glm::mat4 transform;
};

struct ParseInformation {
    std::unordered_map<int32_t, Material> materials;

    std::vector<Bone> bones;
    std::unordered_map<std::string, size_t> boneMap;
};

struct EvaluateInfo {
    Animation animation;
    float time;

    std::vector<glm::mat4> boneTransforms;

    std::vector<Bone> bones;
    std::unordered_map<std::string, size_t> boneMap;

    EvaluateInfo() : animation({}), time(0.f), boneTransforms({}), bones({}), boneMap({}) { }
    EvaluateInfo(float time) : animation({}), time(time), boneTransforms({}), bones({}), boneMap({})
    {
    }
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

Mesh processMesh(
    std::filesystem::path basepath, aiMesh* mesh, const aiScene* scene, ParseInformation& parseInfo)
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;

    std::vector<size_t> boneIDs;

    for (uint32_t i = 0; i < mesh->mNumBones; i++) {
        Bone bone;

        aiBone* meshBone = mesh->mBones[i];

        bone.name = meshBone->mName.C_Str();

        for (uint32_t j = 0; j < meshBone->mNumWeights; j++) {
            aiVertexWeight weight = meshBone->mWeights[j];

            bone.vertexWeights.insert({ weight.mVertexId, weight.mWeight });
        }

        // clang-format off
        aiMatrix4x4 offset = meshBone->mOffsetMatrix;
        bone.offset = {
            offset.a1, offset.b1, offset.c1, offset.d1,
            offset.a2, offset.b2, offset.c2, offset.d2,
            offset.a3, offset.b3, offset.c3, offset.d3,
            offset.a4, offset.b4, offset.c4, offset.d4
        };
        // clang-format on

        size_t boneIndex = parseInfo.bones.size();
        parseInfo.boneMap.insert({ bone.name, boneIndex });
        parseInfo.bones.push_back(bone);

        boneIDs.push_back(boneIndex);
    }

    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        glm::vec4 position = {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z,
            1.f,
        };

        positions.push_back(position);

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

        loadDiffuse(
            basepath, material, aiTextureType_DIFFUSE, parseInfo.materials, mesh->mMaterialIndex);
        matIndex = mesh->mMaterialIndex;
    }

    Mesh returnMesh;
    returnMesh.name = mesh->mName.C_Str();

    for (uint32_t i = 0; i < indices.size() / 3; i++) {
        uint32_t index = i * 3;

        Triangle triangle;
        for (uint32_t v = 0; v < 3; v++) {
            Vertex vertex;

            uint32_t vertexIndex = indices[index + v];
            vertex.position = positions[vertexIndex];
            vertex.texture = glm::vec3(uvs[vertexIndex], 0.f);

            for (const size_t& boneID : boneIDs) {
                const Bone& bone = parseInfo.bones[boneID];

                if (bone.vertexWeights.contains(vertexIndex)) {
                    vertex.boneIDs.push_back(boneID);
                    vertex.boneWeights.push_back(bone.vertexWeights.at(vertexIndex));
                }
            }

            triangle.vertices[v] = vertex;
        }

        triangle.matIndex = matIndex;

        returnMesh.triangles.push_back(triangle);
    }

    return returnMesh;
}

Node parseAssimpNode(
    std::filesystem::path basepath, aiNode* node, const aiScene* scene, ParseInformation& parseInfo)
{
    Node returnNode;
    std::vector<Triangle> triangles;
    returnNode.name = node->mName.C_Str();

    // clang-format off
    aiMatrix4x4 transform = node->mTransformation;
    returnNode.transform = glm::mat4(
            transform.a1, transform.b1, transform.c1, transform.d1,
            transform.a2, transform.b2, transform.c2, transform.d2,
            transform.a3, transform.b3, transform.c3, transform.d3,
            transform.a4, transform.b4, transform.c4, transform.d4
    );
    // clang-format on

    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        Mesh newMesh = processMesh(basepath, mesh, scene, parseInfo);

        returnNode.meshes.push_back(newMesh);
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        Node newNode = parseAssimpNode(basepath, node->mChildren[i], scene, parseInfo);

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
                    { pos.mValue.x, pos.mValue.y, pos.mValue.z },
                });
            }

            for (uint32_t k = 0; k < nodeAnim->mNumRotationKeys; k++) {
                aiQuatKey quat = nodeAnim->mRotationKeys[k];

                channel.rotations.push_back({
                    quat.mTime,
                    { quat.mValue.w, quat.mValue.x, quat.mValue.y, quat.mValue.z },
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

template <typename Type>
glm::mat4 calculateLerp(const std::vector<std::pair<double, Type>>& channel, float target,
    std::function<glm::mat4(Type a, Type b, float t)> lerpFunction)
{
    glm::mat4 returnMat(1.f);
    if (channel.size() != 0) {
        returnMat = lerpFunction(channel[0].second, channel[0].second, 0.0f);

        for (size_t i = 0; i < channel.size() - 1; i++) {
            if (target >= channel[i].first && target < channel[i + 1].first) {
                float t = (target - channel[i].first) / (channel[i + 1].first - channel[i].first);

                return lerpFunction(channel[i].second, channel[i + 1].second, t);
            }
        }
    }

    return returnMat;
}

glm::mat4 evaluateTransform(
    std::string node, const Animation& animation, float time, glm::mat4 transform)
{
    for (auto& channel : animation.channels) {
        if (channel.target != node)
            continue;

        glm::mat4 scale
            = calculateLerp<glm::vec3>(channel.scales, time, [](glm::vec3 a, glm::vec3 b, float t) {
                  glm::vec3 lerp = (1.f - t) * a + t * b;
                  return glm::scale(glm::mat4(1.f), lerp);
              });

        glm::mat4 rotation = calculateLerp<glm::quat>(
            channel.rotations, time, [](glm::quat a, glm::quat b, float t) {
                glm::quat lerp = glm::lerp(a, b, t);
                return glm::mat4_cast(glm::normalize(lerp));
            });

        glm::mat4 translate = calculateLerp<glm::vec3>(
            channel.positions, time, [](glm::vec3 a, glm::vec3 b, float t) {
                glm::vec3 lerp = (1.f - t) * a + t * b;
                return glm::translate(glm::mat4(1.f), lerp);
            });

        transform = transform * translate * rotation * scale;
    }

    return transform;
}

void evaluateBones(const Node& node, EvaluateInfo& info, glm::mat4 transform = glm::mat4(1.f))
{
    if (info.boneMap.contains(node.name)) {
        size_t boneIndex = info.boneMap.at(node.name);
        transform
            = transform * evaluateTransform(node.name, info.animation, info.time, glm::mat4(1.f));

        info.boneTransforms[boneIndex] = transform * info.bones[boneIndex].offset;
    }

    for (const auto& childNode : node.nodes) {
        evaluateBones(childNode, info, transform);
    }
}

std::vector<Triangle> evaluateNodes(
    const Node& node, const EvaluateInfo& info, glm::mat4 transform = glm::mat4(1.f))
{
    std::vector<Triangle> triangles;

    if (!info.boneMap.contains(node.name)) {
        transform = transform * node.transform
            * evaluateTransform(node.name, info.animation, info.time, glm::mat4(1.f));
    }

    // clang-format off
    const glm::mat4 triangleTransform = {
        1.f,  0.f, 0.f, 0.f,
        0.f, -1.f, 0.f, 0.f,
        0.f,  0.f, 1.f, 0.f,
        0.f,  0.f, 0.f, 1.f
    };
    // clang-format on

    for (const auto& mesh : node.meshes) {
        for (size_t i = 0; i < mesh.triangles.size(); i++) {
            Triangle node = mesh.triangles[i];
            if (info.boneMap.size() != 0) {
                node = transformTriangle(node, info.boneTransforms);
            } else {
                node = transformTriangle(node, transform);
            }
            node = transformTriangle(node, triangleTransform);
            triangles.push_back(node);
        }
    }

    for (const auto& node : node.nodes) {
        auto nodeTriangles = evaluateNodes(node, info, transform);
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

    ParseInformation parseInfo;

    std::vector<Animation> animations = parseAnimations(scene);

    Node baseNode = parseAssimpNode(path.parent_path(), scene->mRootNode, scene, parseInfo);

    glm::uvec3 dimensions;
    std::vector<std::unordered_map<glm::ivec3, glm::vec3>> baseVoxels;

    std::vector<std::vector<Triangle>> meshes;

    EvaluateInfo info;
    info.bones = parseInfo.bones;
    info.boneMap = parseInfo.boneMap;
    info.boneTransforms = std::vector<glm::mat4>(info.boneMap.size(), glm::mat4(1.f));

    if (args.animation && animations.size() != 0) {
        Animation animation = animations[0];

        info.animation = animation;

        for (uint32_t frame = 0; frame < args.frames; frame++) {
            float t = (frame / (float)args.frames) * animation.duration;

            info.time = t;
            evaluateBones(baseNode, info, baseNode.transform);

            auto frameTriangles = evaluateNodes(baseNode, info);

            meshes.push_back(frameTriangles);
        }
    } else {
        info.time = 0.f;
        info.animation = {};
        if (animations.size() != 0) {
            info.animation = animations[0];
        }

        evaluateBones(baseNode, info, baseNode.transform);

        meshes.push_back(evaluateNodes(baseNode, info));
    }

    std::tie(dimensions, baseVoxels) = parseMeshes(meshes, parseInfo.materials, args);

    return { dimensions, baseVoxels };
}
}
