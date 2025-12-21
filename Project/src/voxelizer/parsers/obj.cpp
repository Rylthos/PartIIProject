#include "obj.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "pgbar/ProgressBar.hpp"

#include "glm/ext/scalar_constants.hpp"

#include "stb/stb_image.h"

namespace ParserImpl {
bool aabbTriangleSAT(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 aabbSize, glm::vec3 axis)
{
    float p0 = glm::dot(v0, axis);
    float p1 = glm::dot(v1, axis);
    float p2 = glm::dot(v2, axis);

    float r = aabbSize.x * fabs(glm::dot(glm::vec3(1, 0, 0), axis))
        + aabbSize.y * fabs(glm::dot(glm::vec3(0, 1, 0), axis))
        + aabbSize.z * fabs(glm::dot(glm::vec3(0, 0, 1), axis));

    float maxP = fmax(p0, fmax(p1, p2));
    float minP = fmin(p0, fmin(p1, p2));

    return !(fmax(-maxP, minP) > r);
}

bool aabbTriangleIntersection(const Triangle& triangle, glm::vec3 cell, glm::vec3 cellSize)
{
    glm::vec3 cellCenter = cell + cellSize / 2.f;
    glm::vec3 a = triangle.positions[0] - cellCenter;
    glm::vec3 b = triangle.positions[1] - cellCenter;
    glm::vec3 c = triangle.positions[2] - cellCenter;

    glm::vec3 ab = glm::normalize(triangle.positions[1] - triangle.positions[0]);
    glm::vec3 bc = glm::normalize(triangle.positions[2] - triangle.positions[1]);
    glm::vec3 ca = glm::normalize(triangle.positions[0] - triangle.positions[2]);

    glm::vec3 a00 = glm::vec3(0, -ab.z, ab.y);
    glm::vec3 a01 = glm::vec3(0, -bc.z, bc.y);
    glm::vec3 a02 = glm::vec3(0, -ca.z, ca.y);

    glm::vec3 a10 = glm::vec3(ab.z, 0, -ab.x);
    glm::vec3 a11 = glm::vec3(bc.z, 0, -bc.x);
    glm::vec3 a12 = glm::vec3(ca.z, 0, -ca.x);

    glm::vec3 a20 = glm::vec3(-ab.y, ab.z, 0);
    glm::vec3 a21 = glm::vec3(-bc.y, bc.z, 0);
    glm::vec3 a22 = glm::vec3(-ca.y, ca.z, 0);

    return aabbTriangleSAT(a, b, c, cellSize, a00) && aabbTriangleSAT(a, b, c, cellSize, a01)
        && aabbTriangleSAT(a, b, c, cellSize, a02) && aabbTriangleSAT(a, b, c, cellSize, a10)
        && aabbTriangleSAT(a, b, c, cellSize, a11) && aabbTriangleSAT(a, b, c, cellSize, a12)
        && aabbTriangleSAT(a, b, c, cellSize, a20) && aabbTriangleSAT(a, b, c, cellSize, a21)
        && aabbTriangleSAT(a, b, c, cellSize, a22)
        && aabbTriangleSAT(a, b, c, cellSize, glm::vec3(1, 0, 0))
        && aabbTriangleSAT(a, b, c, cellSize, glm::vec3(0, 1, 0))
        && aabbTriangleSAT(a, b, c, cellSize, glm::vec3(0, 0, 1))
        && aabbTriangleSAT(a, b, c, cellSize, glm::cross(ab, bc));
}

glm::vec3 triangleClosestPoint(const Triangle& triangle, glm::vec3 original)
{
    const glm::vec3 a = triangle.positions[0];
    const glm::vec3 b = triangle.positions[1];
    const glm::vec3 c = triangle.positions[2];

    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;

    glm::vec3 normal = glm::cross(ab, ac);

    float dist = glm::dot(normal, original - a);
    glm::vec3 point = original - normal * dist;

    const glm::vec3 ap = point - a;

    const float d1 = dot(ab, ap);
    const float d2 = dot(ac, ap);
    if (d1 <= 0.f && d2 <= 0.f)
        return a;

    const glm::vec3 bp = point - b;
    const float d3 = dot(ab, bp);
    const float d4 = dot(ac, bp);
    if (d3 >= 0.f && d4 <= d3)
        return b;

    const glm::vec3 cp = point - c;
    const float d5 = dot(ab, cp);
    const float d6 = dot(ac, cp);
    if (d6 >= 0.f && d5 <= d6)
        return c;

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
        const float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
        const float v = d2 / (d2 - d6);
        return a + v * ac;
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
        const float v = (d4 - d3) / ((d3 - d3) + (d5 - d6));
        return b + v * (c - b);
    }

    const float denom = 1.f / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    return a + v * ab + w * ac;
}

glm::vec3 calculateTexCoords(const Triangle& triangle, glm::vec3 cell, glm::vec3 cellSize)
{
    glm::vec3 point = triangleClosestPoint(triangle, cell + cellSize / 2.f);

    const glm::vec3 a = triangle.positions[0];
    const glm::vec3 b = triangle.positions[1];
    const glm::vec3 c = triangle.positions[2];

    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    glm::vec3 N = glm::cross(ab, ac);
    float denom = glm::dot(N, N);

    glm::vec3 bp = point - b;
    glm::vec3 bc = c - b;

    glm::vec3 C = glm::cross(bc, bp);
    float u = glm::dot(N, C);

    glm::vec3 cp = point - c;
    glm::vec3 ca = a - c;
    C = glm::cross(ca, cp);
    float v = glm::dot(N, C);

    u /= denom;
    v /= denom;

    return glm::mod(
        u * triangle.texture[0] + v * triangle.texture[1] + (1.f - u - v) * triangle.texture[2],
        glm::vec3(1.0f));
}

void parseImage(std::filesystem::path filepath, Material& material)
{
    stbi_set_flip_vertically_on_load(true);
    material.data = stbi_load(
        filepath.string().c_str(), &material.width, &material.height, &material.colourDepth, 0);

    if (!material.data) {
        fprintf(stderr, "Failed to load %s\n", filepath.string().c_str());
        exit(-1);
    }
}

void parseMaterialLib(
    std::filesystem::path filepath, std::unordered_map<std::string, Material>& materials)
{
    std::ifstream materialFile(filepath.string());
    if (!materialFile) {
        fprintf(stderr, "Failed to open material lib: %s\n", filepath.string().c_str());
        exit(-1);
    }

    std::string currentMaterial = "";
    Material material;

    std::string line;
    while (std::getline(materialFile, line)) {
        if (line[0] == '#')
            continue;
        if (line.length() == 0)
            continue;

        auto codePosEnd = line.find(" ");
        std::string code = line.substr(0, codePosEnd);
        std::string arguments = line.substr(codePosEnd + 1);

        if (code[0] == '\t') {
            code = code.erase(0, 1);
        }

        if (code == "newmtl") {
            if (currentMaterial != "")
                materials[currentMaterial] = material;

            currentMaterial = arguments;
        } else if (code == "Ns") {
        } else if (code == "Kd") {
            auto items = split(arguments, " ");
            material.diffuse = glm::vec3(0);
            int index = 0;
            for (auto item : items) {
                material.diffuse[index++] = atof(item.c_str());
            }
        } else if (code == "Ke") {
        } else if (code == "Ni") {
        } else if (code == "d") {
        } else if (code == "illum") {
        } else if (code == "map_Kd") {
            material.validTexture = true;
            parseImage(filepath.parent_path() / arguments, material);
        }
    }

    if (currentMaterial != "")
        materials[currentMaterial] = material;
}

ParserRet parseObj(std::filesystem::path filepath, const ParserArgs& args)
{
    std::vector<Triangle> triangles;

    std::map<uint32_t, std::string> indexToMaterial;
    std::map<std::string, uint32_t> materialToIndex;

    std::unordered_map<std::string, Material> materials;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> texture;

    std::ifstream file(filepath.c_str());
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", filepath.c_str());
        exit(-1);
    }

    uint32_t currentMaterial = -1;

    std::string line;
    while (std::getline(file, line)) {
        if (line[0] == '#')
            continue;
        if (line.length() == 0)
            continue;

        auto codePosEnd = line.find(" ");
        std::string code = line.substr(0, codePosEnd);
        std::string arguments = line.substr(codePosEnd + 1);

        if (code == "v") { // Vertices
            glm::vec4 vertex { 0, 0, 0, 1 };

            auto lineVertices = split(arguments, " ");

            int index = 0;
            for (const auto& v : lineVertices) {
                vertex[index++] = std::atof(v.c_str());
            }

            vertices.push_back(glm::vec3(vertex.x, -vertex.y, -vertex.z));
        } else if (code == "vt") { // Vertex Textures
            glm::vec3 tex = { 0., 0., 0. };

            auto lineTextures = split(arguments, " ");

            int index = 0;
            for (const auto& t : lineTextures) {
                tex[index++] = std::atof(t.c_str());
            }

            texture.push_back(tex);
        } else if (code == "vn") { // Vertex Normals
            // Normals ignored
        } else if (code == "f") { // Face
            std::vector<std::tuple<int, int, int>> faces;

            auto lineFaces = split(arguments, " ");

            bool textures = false;
            for (auto f : lineFaces) {
                auto elements = split(f, "/");

                int vertex = -1;
                int texture = -1;
                int normal = -1;

                vertex = std::atoi(elements[0].c_str()) - 1;
                if (elements.size() >= 2 && elements[1].length() != 0) {
                    texture = std::atoi(elements[1].c_str()) - 1;
                    textures = true;
                }
                if (elements.size() >= 3)
                    normal = std::atoi(elements[2].c_str()) - 1;

                faces.push_back(std::make_tuple(vertex, texture, normal));
            }

            assert(faces.size() >= 3 && faces.size() <= 4 && "Require atleast 3 vertex defines");

            if (faces.size() == 3) {
                Triangle t;
                auto v1 = faces[0];
                auto v2 = faces[1];
                auto v3 = faces[2];

                t.positions[0] = vertices[std::get<0>(v1)];
                t.positions[1] = vertices[std::get<0>(v2)];
                t.positions[2] = vertices[std::get<0>(v3)];

                if (textures) {
                    t.texture[0] = texture[std::get<1>(v1)];
                    t.texture[1] = texture[std::get<1>(v2)];
                    t.texture[2] = texture[std::get<1>(v3)];
                }

                t.matIndex = currentMaterial;

                triangles.push_back(t);
            } else if (faces.size() == 4) {
                Triangle t1;
                Triangle t2;
                auto v1 = faces[0];
                auto v2 = faces[1];
                auto v3 = faces[2];
                auto v4 = faces[3];

                t1.positions[0] = vertices[std::get<0>(v1)];
                t1.positions[1] = vertices[std::get<0>(v2)];
                t1.positions[2] = vertices[std::get<0>(v3)];

                t2.positions[0] = vertices[std::get<0>(v3)];
                t2.positions[1] = vertices[std::get<0>(v4)];
                t2.positions[2] = vertices[std::get<0>(v1)];

                if (textures) {
                    t1.texture[0] = texture[std::get<1>(v1)];
                    t1.texture[1] = texture[std::get<1>(v2)];
                    t1.texture[2] = texture[std::get<1>(v3)];

                    t2.texture[0] = texture[std::get<1>(v3)];
                    t2.texture[1] = texture[std::get<1>(v4)];
                    t2.texture[2] = texture[std::get<1>(v1)];
                }

                t1.matIndex = currentMaterial;
                t2.matIndex = currentMaterial;

                triangles.push_back(t1);
                triangles.push_back(t2);
            }
        } else if (code == "mtllib") {
            std::string name = arguments;
            parseMaterialLib(filepath.parent_path() / name, materials);
        } else if (code == "usemtl") {
            if (materialToIndex.contains(arguments)) {
                currentMaterial = materialToIndex[arguments];
            } else {
                uint32_t index = materialToIndex.size();
                materialToIndex[arguments] = index;
                indexToMaterial[index] = arguments;
                currentMaterial = index;
            }
        } else if (code == "s") {
            // No smooth shading
        } else if (code == "o") {
            // Dont currently handle multiple objects
        } else if (code == "g") {
            // Dont currently handle multiple objects
        } else {
            fprintf(stderr, "[ERROR]: Unsupported element of OBJ: %s\n", code.c_str());
            exit(-1);
        }
    }

    return parseMesh(triangles, materials, indexToMaterial, args);
}

ParserRet parseMesh(const std::vector<Triangle>& triangles,
    const std::unordered_map<std::string, Material>& materials,
    const std::map<uint32_t, std::string>& indexToMaterial, const ParserArgs& args)
{
    std::unordered_map<glm::ivec3, glm::vec3> voxels;

    glm::vec3 minBound(10000000);
    glm::vec3 maxBound(-10000000);

    for (size_t i = 0; i < triangles.size(); i++) {
        for (size_t j = 0; j < 3; j++) {
            maxBound = glm::max(maxBound, triangles[i].positions[j]);
            minBound = glm::min(minBound, triangles[i].positions[j]);
        }
    }

    glm::vec3 size = glm::max(maxBound - minBound, glm::vec3(glm::epsilon<float>()));
    float maxSide = fmax(size.x, fmax(size.y, size.z));
    glm::vec3 aspect = size / maxSide;

    // World space to voxel space
    glm::vec3 scalar = (aspect * (float)args.voxels_per_unit * args.units) / size;

    glm::uvec3 dimensions = glm::max(glm::uvec3(glm::ceil(size * scalar)), glm::uvec3(1));

    glm::vec3 cellSize = glm::vec3(1.f) / scalar;
    pgbar::ProgressBar<pgbar::Channel::Stderr, pgbar::Policy::Async, pgbar::Region::Relative> bar;

    bar.config().tasks(triangles.size());
    bar.config().enable().percent().elapsed().countdown();
    bar.config().disable().speed();
    bar.config().prefix("Voxelizing triangles");

    for (const auto& t : triangles) {
        glm::uvec3 triangleMin = glm::floor(
            (glm::min(t.positions[0], glm::min(t.positions[1], t.positions[2])) - minBound)
            * scalar);
        glm::uvec3 triangleMax = glm::max(
            glm::uvec3(glm::ceil(
                (glm::max(t.positions[0], glm::max(t.positions[1], t.positions[2])) - minBound)
                * scalar)),
            glm::uvec3(1));

        for (int z = triangleMin.z; z < triangleMax.z; z++) {
            for (int y = triangleMin.y; y < triangleMax.y; y++) {
                for (int x = triangleMin.x; x < triangleMax.x; x++) {
                    glm::ivec3 index = glm::ivec3(x, y, z);
                    glm::vec3 cubeMin = (glm::vec3(index) / scalar) + minBound;

                    if (aabbTriangleIntersection(t, cubeMin, cellSize)) {
                        if (t.matIndex != -1) {
                            const Material& mat = materials.at(indexToMaterial.at(t.matIndex));
                            if (mat.validTexture) {
                                glm::vec3 tex = calculateTexCoords(t, cubeMin, cellSize);

                                int x = std::clamp((int)(tex.x * mat.width), 0, mat.width - 1);
                                int y = std::clamp((int)(tex.y * mat.height), 0, mat.height - 1);
                                size_t colourIndex = (x + y * mat.width) * mat.colourDepth;

                                glm::vec3 colour = {
                                    mat.data[colourIndex + 0] / 255.f,
                                    mat.data[colourIndex + 1] / 255.f,
                                    mat.data[colourIndex + 2] / 255.f,
                                };

                                voxels[index] = colour;
                            } else {
                                voxels[index] = mat.diffuse;
                            }
                        } else {
                            voxels[index] = glm::vec3(1);
                        }
                    }
                }
            }
        }

        bar.tick();
    }

    bar.reset();

    return std::make_tuple(dimensions, voxels);
}

}
