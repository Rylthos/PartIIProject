#include "parser.hpp"

#include "generators/brickmap.hpp"
#include "generators/common.hpp"
#include "generators/contree.hpp"
#include "generators/octree.hpp"
#include "generators/texture.hpp"
#include "loaders/sparse_loader.hpp"

#include "generators/grid.hpp"

#include "pgbar/details/core/Core.hpp"

#include "serializers/brickmap.hpp"
#include "serializers/contree.hpp"
#include "serializers/grid.hpp"
#include "serializers/octree.hpp"

#include <glm/gtx/string_cast.hpp>

#include "pgbar/DynamicBar.hpp"
#include "pgbar/ProgressBar.hpp"
#include "serializers/texture.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>

static std::map<Structure, const char*> structureToString {
    { GRID,     "[Grid]    " },
    { TEXTURE,  "[Texture] " },
    { OCTREE,   "[Octree]  " },
    { CONTREE,  "[Contree] " },
    { BRICKMAP, "[Brickmap]" },
};

std::vector<std::string> split(std::string str, std::string delim)
{
    std::vector<std::string> components;

    size_t pos = 0;
    size_t newPos = 0;
    while (newPos != std::string::npos) {
        newPos = str.find(delim, pos);
        std::string sect = str.substr(pos, newPos - pos);
        pos = newPos + 1;

        components.push_back(sect);
    }

    return components;
}

Parser::Parser(ParserArgs args) : m_Args(args)
{
    if (m_Args.flag_all || m_Args.flag_grid)
        m_ValidStructures[GRID] = true;
    if (m_Args.flag_all || m_Args.flag_texture)
        m_ValidStructures[TEXTURE] = true;
    if (m_Args.flag_all || m_Args.flag_octree)
        m_ValidStructures[OCTREE] = true;
    if (m_Args.flag_all || m_Args.flag_contree)
        m_ValidStructures[CONTREE] = true;
    if (m_Args.flag_all || m_Args.flag_brickmap)
        m_ValidStructures[BRICKMAP] = true;

    parseFile();
    parseMesh();
    generateStructures();
}

void Parser::parseFile()
{
    std::filesystem::path path = m_Args.filename;
    auto extension = path.extension();

    if (!strcmp(extension.c_str(), ".obj")) {
        parseObj(path);
    } else {
        fprintf(stderr, "Unsupported file\n");
        exit(-1);
    }
}

void Parser::parseObj(std::filesystem::path filepath)
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> texture;

    std::ifstream file(filepath.c_str());
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", filepath.c_str());
        exit(-1);
    }
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

            assert(faces.size() >= 3 && "Require atleast 3 vertex defines");

            for (size_t i = 2; i < faces.size(); i++) {
                Triangle t;
                auto v1 = faces[i - 2];
                auto v2 = faces[i - 1];
                auto v3 = faces[i];
                t.positions[0] = vertices[std::get<0>(v1)];
                t.positions[1] = vertices[std::get<0>(v2)];
                t.positions[2] = vertices[std::get<0>(v3)];

                if (textures) {
                    t.texture[0] = texture[std::get<1>(v1)];
                    t.texture[1] = texture[std::get<1>(v2)];
                    t.texture[2] = texture[std::get<1>(v3)];
                }

                m_Triangles.push_back(t);
            }

        } else if (code == "mtllib") {
            fprintf(stderr, "[WARNING]: Materials not supported\n");
        } else if (code == "usemtl") {
            fprintf(stderr, "[WARNING]: Materials not supported\n");
        } else if (code == "s") {
            if (arguments == "1" || arguments == "on")
                fprintf(stderr, "[ERROR]: Smooth shading not supported\n");
        } else if (code == "o") {
            // Dont currently handle multiple objects
        } else {
            fprintf(stderr, "[ERROR]: Unsupported element of OBJ: %s\n", code.c_str());
            exit(-1);
        }
    }
}

void Parser::parseMesh()
{
    glm::vec3 minBound(10000000);
    glm::vec3 maxBound(-10000000);

    for (size_t i = 0; i < m_Triangles.size(); i++) {
        for (size_t j = 0; j < 3; j++) {
            maxBound = glm::max(maxBound, m_Triangles[i].positions[j]);
            minBound = glm::min(minBound, m_Triangles[i].positions[j]);
        }
    }

    glm::vec3 size = maxBound - minBound;
    float maxSide = fmax(size.x, fmax(size.y, size.z));
    glm::vec3 aspect = size / maxSide;
    glm::vec3 scalar = (aspect * (float)m_Args.voxels_per_unit * m_Args.units) / size;

    m_Dimensions = glm::max(glm::uvec3(glm::ceil(size * scalar)), glm::uvec3(1));

    for (auto t : m_Triangles) {
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
                    glm::vec3 cubeMin = (glm::vec3(x, y, z) / scalar) + minBound;
                    glm::ivec3 index = glm::ivec3(x, y, z);

                    if (aabbTriangleIntersection(
                            t, cubeMin, glm::vec3(1.f / m_Args.voxels_per_unit))) {
                        m_Voxels[index] = glm::vec3(1);
                    }
                }
            }
        }
    }
}

void Parser::generateStructures()
{
    std::filesystem::path outputDirectory = m_Args.output;
    std::string outputName = m_Args.name;

    if (outputName.length() == 0) {
        outputName = std::filesystem::path(m_Args.filename).filename();
    }

    printf("Output directory: %s\n", (outputDirectory / outputName).string().c_str());
    if (!std::filesystem::exists(outputDirectory / outputName)) {
        std::filesystem::create_directory(outputDirectory / outputName);
    }

    printf("Voxel dimensions: %s\n", glm::to_string(m_Dimensions).c_str());

    std::jthread threads[AS_COUNT];
    Generators::GenerationInfo info[AS_COUNT] {};
    bool finished[AS_COUNT];

    if (m_ValidStructures[GRID]) {
        threads[GRID] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(m_Dimensions, m_Voxels);
            glm::uvec3 dimensions;

            auto voxels = Generators::generateGrid(
                stoken, std::move(loader), info[GRID], dimensions, finished[GRID]);

            Serializers::storeGrid(outputDirectory, outputName, dimensions, voxels, info[GRID]);
        });
    }

    if (m_ValidStructures[TEXTURE]) {
        threads[TEXTURE] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(m_Dimensions, m_Voxels);
            glm::uvec3 dimensions;
            auto nodes = Generators::generateTexture(
                stoken, std::move(loader), info[TEXTURE], dimensions, finished[TEXTURE]);

            Serializers::storeTexture(
                outputDirectory, outputName, dimensions, nodes, info[TEXTURE]);
        });
    }

    if (m_ValidStructures[OCTREE]) {
        threads[OCTREE] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(m_Dimensions, m_Voxels);
            glm::uvec3 dimensions;
            auto nodes = Generators::generateOctree(
                stoken, std::move(loader), info[OCTREE], dimensions, finished[OCTREE]);

            Serializers::storeOctree(outputDirectory, outputName, dimensions, nodes, info[OCTREE]);
        });
    }

    if (m_ValidStructures[CONTREE]) {
        threads[CONTREE] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(m_Dimensions, m_Voxels);
            glm::uvec3 dimensions;
            auto nodes = Generators::generateContree(
                stoken, std::move(loader), info[CONTREE], dimensions, finished[CONTREE]);

            Serializers::storeContree(outputDirectory, outputName, dimensions, nodes, info[OCTREE]);
        });
    }

    if (m_ValidStructures[BRICKMAP]) {
        threads[BRICKMAP] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(m_Dimensions, m_Voxels);
            glm::uvec3 dimensions;
            std::vector<Generators::BrickgridPtr> brickgrid;
            std::vector<Generators::Brickmap> brickmaps;
            std::tie(brickgrid, brickmaps) = Generators::generateBrickmap(
                stoken, std::move(loader), info[BRICKMAP], dimensions, finished[BRICKMAP]);

            Serializers::storeBrickmap(
                outputDirectory, outputName, dimensions, brickgrid, brickmaps, info[BRICKMAP]);
        });
    }

    pgbar::DynamicBar<pgbar::Channel::Stderr, pgbar::Policy::Async, pgbar::Region::Relative>
        dynamicBar;
    std::map<size_t, std::thread> barPool;
    for (size_t i = 0; i < AS_COUNT; i++) {
        if (!m_ValidStructures[i]) {
            continue;
        }

        barPool[i] = std::thread([i, &info, &finished, &dynamicBar]() {
            auto bar = dynamicBar.insert(pgbar::config::Line(pgbar::option::Tasks(10000)));

            bar->config().enable().percent().elapsed().countdown();
            bar->config().disable().speed().counter();
            bar->config().prefix(structureToString[(Structure)i]);

            float prev = 0.0f;
            do {
                if (info[i].completionPercent >= 1.f) {
                    bar->tick_to(100);
                } else {
                    if (fabs(info[i].completionPercent - prev) > 0.0001) {
                        bar->tick((info[i].completionPercent - prev) * 10000);
                        prev = info[i].completionPercent;
                    }
                }
            } while (!finished[i]);
        });
    }

    for (size_t i = 0; i < AS_COUNT; i++) {
        if (m_ValidStructures[i]) {
            threads[i].join();
            barPool[i].join();
        }
    }
}

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

bool Parser::aabbTriangleIntersection(Triangle triangle, glm::vec3 cell, glm::vec3 cellSize)
{
    glm::vec3 cellCenter = cell + cellSize / 2.f;
    glm::vec3 v0 = triangle.positions[0] - cellCenter;
    glm::vec3 v1 = triangle.positions[1] - cellCenter;
    glm::vec3 v2 = triangle.positions[2] - cellCenter;

    glm::vec3 ab = glm::normalize(triangle.positions[1] - triangle.positions[0]);
    glm::vec3 bc = glm::normalize(triangle.positions[2] - triangle.positions[1]);
    glm::vec3 ca = glm::normalize(triangle.positions[0] - triangle.positions[2]);

    glm::vec3 a00 = glm::vec3(0, -ab.z, ab.y);
    glm::vec3 a01 = glm::vec3(0, -bc.z, bc.y);
    glm::vec3 a02 = glm::vec3(0, -ca.z, ca.y);

    glm::vec3 a10 = glm::vec3(ab.z, 0.0, ab.x);
    glm::vec3 a11 = glm::vec3(bc.z, 0.0, bc.x);
    glm::vec3 a12 = glm::vec3(ca.z, 0.0, ca.x);

    glm::vec3 a20 = glm::vec3(-ab.y, ab.x, 0.0);
    glm::vec3 a21 = glm::vec3(-bc.y, bc.x, 0.0);
    glm::vec3 a22 = glm::vec3(-ca.y, ca.x, 0.0);

    return !(!aabbTriangleSAT(v0, v1, v2, cellSize, a00)
        || !aabbTriangleSAT(v0, v1, v2, cellSize, a01)
        || !aabbTriangleSAT(v0, v1, v2, cellSize, a02)
        || !aabbTriangleSAT(v0, v1, v2, cellSize, a10)
        || !aabbTriangleSAT(v0, v1, v2, cellSize, a11)
        || !aabbTriangleSAT(v0, v1, v2, cellSize, a12)
        || !aabbTriangleSAT(v0, v1, v2, cellSize, a20)
        || !aabbTriangleSAT(v0, v1, v2, cellSize, a21)
        || !aabbTriangleSAT(v0, v1, v2, cellSize, a22)
        || !aabbTriangleSAT(v0, v1, v2, cellSize, glm::vec3(1, 0, 0))
        || !aabbTriangleSAT(v0, v1, v2, cellSize, glm::vec3(0, 1, 0))
        || !aabbTriangleSAT(v0, v1, v2, cellSize, glm::vec3(0, 0, 1))
        || !aabbTriangleSAT(v0, v1, v2, cellSize, glm::cross(ab, bc)));
}
