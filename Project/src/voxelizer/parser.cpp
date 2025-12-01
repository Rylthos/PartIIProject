#include "parser.hpp"

#include <glm/gtx/string_cast.hpp>

#include <cstring>
#include <fstream>
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

            vertices.push_back(glm::vec3(vertex.x, vertex.y, vertex.z));
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
