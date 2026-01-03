#include "general.hpp"
#include "glm/ext/scalar_constants.hpp"
#include "pgbar/ProgressBar.hpp"

#include <algorithm>

#include <stb/stb_image.h>

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

std::vector<std::string> split(std::string str, std::string delim)
{
    std::vector<std::string> components;

    size_t pos = 0;
    size_t newPos = 0;
    while (newPos != std::string::npos) {
        newPos = str.find(delim, pos);
        std::string sect = str.substr(pos, newPos - pos);
        pos = newPos + 1;

        if (sect.length() != 0) {
            components.push_back(sect);
        } else {
            components.push_back("");
        }
    }

    return components;
}

ParserRet parseMesh(const std::vector<Triangle>& triangles,
    const std::unordered_map<int32_t, Material>& materials, const ParserArgs& args)
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
                            const Material& mat = materials.at(t.matIndex);
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

    return std::make_tuple(dimensions, std::vector { voxels });
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

}
