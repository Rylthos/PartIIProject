#include "parser.hpp"

#include "loaders/sparse_loader.hpp"

#include "generators/grid.hpp"

#include "serializers/grid.hpp"

#include <filesystem>
#include <glm/gtx/string_cast.hpp>

#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

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
    std::string line;
    while (std::getline(file, line)) {
        if (line[0] == '#')
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

            vertices.push_back(glm::vec3(vertex.x, vertex.z, vertex.y));
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

            for (auto f : lineFaces) {
                auto elements = split(f, "/");

                int vertex = -1;
                int texture = -1;
                int normal = -1;

                vertex = std::atoi(elements[0].c_str()) - 1;
                if (elements.size() >= 2 && elements[1].length() != 0)
                    texture = std::atoi(elements[1].c_str()) - 1;
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

                t.texture[0] = texture[std::get<1>(v1)];
                t.texture[1] = texture[std::get<1>(v2)];
                t.texture[2] = texture[std::get<1>(v3)];

                m_Triangles.push_back(t);
            }

        } else if (code == "mtllib") {
            fprintf(stderr, "[WARNING]: Materials not supported\n");
        } else if (code == "usemtl") {
            fprintf(stderr, "[WARNING]: Materials not supported\n");
        } else if (code == "s") {
            if (arguments == "1" || arguments == "on")
                fprintf(stderr, "[ERROR]: Smooth shading not supported");
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

    m_Dimensions = glm::max(
        glm::uvec3(glm::ceil((maxBound - minBound) * m_Args.voxels_per_unit)), glm::uvec3(1));

    for (auto t : m_Triangles) {
        glm::vec3 triangleMin = glm::floor(
            (glm::min(t.positions[0], glm::min(t.positions[1], t.positions[2])) - minBound)
            * m_Args.voxels_per_unit);
        glm::vec3 triangleMax = glm::max(
            glm::ceil(
                (glm::max(t.positions[0], glm::max(t.positions[1], t.positions[2])) - minBound)
                * m_Args.voxels_per_unit),
            glm::vec3(1));

        for (int z = triangleMin.z; z < triangleMax.z; z++) {
            for (int y = triangleMin.y; y < triangleMax.y; y++) {
                for (int x = triangleMin.x; x < triangleMax.x; x++) {
                    glm::vec3 cubeMin = glm::vec3(x, y, z) / m_Args.voxels_per_unit;

                    // Bad detection
                    // TODO: Actually implement the code
                    glm::ivec3 index = glm::ivec3(x, y, z);
                    m_Voxels[index] = glm::vec3(1);
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

    if (m_Args.flag_all || m_Args.flag_grid) {
        std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(m_Dimensions, m_Voxels);
        Generators::GenerationInfo info;
        glm::uvec3 dimensions;
        bool finished;
        auto thread = std::jthread([&, loader = std::move(loader)](std::stop_token stoken) mutable {
            auto voxels
                = Generators::generateGrid(stoken, std::move(loader), info, dimensions, finished);

            printf("%s\n", glm::to_string(dimensions).c_str());
            printf("Time: %f\n", info.generationTime);

            Serializers::storeGrid(outputDirectory, outputName, dimensions, voxels);
        });

        thread.join();
    }
}
