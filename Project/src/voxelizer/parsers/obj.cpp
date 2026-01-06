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

                t.vertices[0].position = vertices[std::get<0>(v1)];
                t.vertices[1].position = vertices[std::get<0>(v2)];
                t.vertices[2].position = vertices[std::get<0>(v3)];

                if (textures) {
                    t.vertices[0].texture = texture[std::get<1>(v1)];
                    t.vertices[1].texture = texture[std::get<1>(v2)];
                    t.vertices[2].texture = texture[std::get<1>(v3)];
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

                t1.vertices[0].position = vertices[std::get<0>(v1)];
                t1.vertices[1].position = vertices[std::get<0>(v2)];
                t1.vertices[2].position = vertices[std::get<0>(v3)];

                t2.vertices[0].position = vertices[std::get<0>(v3)];
                t2.vertices[1].position = vertices[std::get<0>(v4)];
                t2.vertices[2].position = vertices[std::get<0>(v1)];

                if (textures) {
                    t1.vertices[0].texture = texture[std::get<1>(v1)];
                    t1.vertices[1].texture = texture[std::get<1>(v2)];
                    t1.vertices[2].texture = texture[std::get<1>(v3)];

                    t2.vertices[0].texture = texture[std::get<1>(v3)];
                    t2.vertices[1].texture = texture[std::get<1>(v4)];
                    t2.vertices[2].texture = texture[std::get<1>(v1)];
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

    std::unordered_map<int32_t, Material> mappedMaterials;
    for (const auto& material : materials) {
        mappedMaterials.insert({ materialToIndex[material.first], material.second });
    }

    return parseMesh(triangles, mappedMaterials, args);
}

}
