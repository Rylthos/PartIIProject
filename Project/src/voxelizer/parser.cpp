#include "parser.hpp"

#include "generators/brickmap.hpp"
#include "generators/common.hpp"
#include "generators/contree.hpp"
#include "generators/octree.hpp"
#include "generators/texture.hpp"
#include "loaders/sparse_loader.hpp"

#include "generators/grid.hpp"

#include "modification/diff.hpp"
#include "serializers/brickmap.hpp"
#include "serializers/contree.hpp"
#include "serializers/grid.hpp"
#include "serializers/octree.hpp"

#include <glm/gtx/string_cast.hpp>

#include "serializers/texture.hpp"

#include "parsers/assimp.hpp"
#include "parsers/general.hpp"
#include "parsers/obj.hpp"
#include "parsers/vox.hpp"

#include "pgbar/DynamicBar.hpp"
#include "pgbar/ProgressBar.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

static std::map<Structure, const char*> structureToString {
    { GRID,     "[Grid]    " },
    { TEXTURE,  "[Texture] " },
    { OCTREE,   "[Octree]  " },
    { CONTREE,  "[Contree] " },
    { BRICKMAP, "[Brickmap]" },
};

Parser::Parser(ParserArgs args) : m_Args(args)
{
    memset(m_ValidStructures, 0, AS_COUNT * sizeof(bool));

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

    glm::uvec3 dimensions;
    std::vector<std::unordered_map<glm::ivec3, glm::vec3>> frames;
    std::tie(dimensions, frames) = parseFile();
    generateStructures(dimensions, frames);
}

Parser::~Parser()
{
    // Technically required
    // for (auto& material : m_Materials) {
    //     stbi_image_free(material.second.data);
    // }
}

ParserImpl::ParserRet Parser::parseFile()
{
    std::filesystem::path path = m_Args.filename;
    auto extension = path.extension();

    if (!strcmp(extension.c_str(), ".obj")) {
        return ParserImpl::parseObj(path, m_Args);
    } else if (!strcmp(extension.c_str(), ".vox")) {
        return ParserImpl::parseVox(path, m_Args);
    } else if (!strcmp(extension.c_str(), ".gltf") || !strcmp(extension.c_str(), ".glb")) {
        return ParserImpl::parseAssimp(path, m_Args);
    } else {
        fprintf(stderr, "Unsupported file\n");
        exit(-1);
    }
}

void Parser::generateStructures(
    glm::uvec3 dimensions, const std::vector<std::unordered_map<glm::ivec3, glm::vec3>>& frames)
{
    Modification::AnimationFrames animationFrames;
    if (m_Args.animation) {
        animationFrames = generateAnimations(frames, dimensions);
    }

    std::filesystem::path outputDirectory = m_Args.output;
    std::string outputName = m_Args.name;

    if (outputName.length() == 0) {
        outputName = std::filesystem::path(m_Args.filename).filename();
    }

    printf("Output directory: %s\n", (outputDirectory / outputName).string().c_str());
    if (!std::filesystem::exists(outputDirectory / outputName)) {
        std::filesystem::create_directory(outputDirectory / outputName);
    }

    printf("Voxel dimensions: %s\n", glm::to_string(dimensions).c_str());

    std::jthread threads[AS_COUNT];
    Generators::GenerationInfo info[AS_COUNT] {};
    bool finished[AS_COUNT];

    if (m_ValidStructures[GRID]) {
        threads[GRID] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(dimensions, frames[0]);
            glm::uvec3 dimensions;

            auto voxels = Generators::generateGrid(
                stoken, std::move(loader), info[GRID], dimensions, finished[GRID]);

            Serializers::storeGrid(
                outputDirectory, outputName, dimensions, voxels, info[GRID], animationFrames);
        });
    }

    if (m_ValidStructures[TEXTURE]) {
        threads[TEXTURE] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(dimensions, frames[0]);
            glm::uvec3 dimensions;
            auto nodes = Generators::generateTexture(
                stoken, std::move(loader), info[TEXTURE], dimensions, finished[TEXTURE]);

            Serializers::storeTexture(
                outputDirectory, outputName, dimensions, nodes, info[TEXTURE], animationFrames);
        });
    }

    if (m_ValidStructures[OCTREE]) {
        threads[OCTREE] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(dimensions, frames[0]);
            glm::uvec3 dimensions;
            auto nodes = Generators::generateOctree(
                stoken, std::move(loader), info[OCTREE], dimensions, finished[OCTREE]);

            Serializers::storeOctree(outputDirectory, outputName, dimensions, nodes, info[OCTREE]);
        });
    }

    if (m_ValidStructures[CONTREE]) {
        threads[CONTREE] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(dimensions, frames[0]);
            glm::uvec3 dimensions;
            auto nodes = Generators::generateContree(
                stoken, std::move(loader), info[CONTREE], dimensions, finished[CONTREE]);

            Serializers::storeContree(outputDirectory, outputName, dimensions, nodes, info[OCTREE]);
        });
    }

    if (m_ValidStructures[BRICKMAP]) {
        threads[BRICKMAP] = std::jthread([&](std::stop_token stoken) {
            std::unique_ptr<Loader> loader = std::make_unique<SparseLoader>(dimensions, frames[0]);
            glm::uvec3 dimensions;
            std::vector<Generators::BrickgridPtr> brickgrid;
            std::vector<Generators::Brickmap> brickmaps;
            std::vector<Generators::BrickmapColour> colours;
            std::tie(brickgrid, brickmaps, colours) = Generators::generateBrickmap(
                stoken, std::move(loader), info[BRICKMAP], dimensions, finished[BRICKMAP]);

            Serializers::storeBrickmap(outputDirectory, outputName, dimensions, brickgrid,
                brickmaps, colours, info[BRICKMAP], animationFrames);
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

Modification::AnimationFrames Parser::generateAnimations(
    const std::vector<std::unordered_map<glm::ivec3, glm::vec3>>& frames, glm::uvec3 dimensions)
{
    if (frames.size() == 1) {
        return {};
    }

    Modification::AnimationFrames animation;

    const size_t frameCount = frames.size();

    auto getVoxel = [](const std::unordered_map<glm::ivec3, glm::vec3>& frames, glm::ivec3 index) {
        if (frames.contains(index)) {
            return std::make_optional<glm::vec3>(frames.at(index));
        } else {
            return std::optional<glm::vec3>();
        }
    };

    animation.resize(frameCount);

    for (uint32_t y = 0; y < dimensions.y; y++) {
        for (uint32_t z = 0; z < dimensions.z; z++) {
            for (uint32_t x = 0; x < dimensions.x; x++) {
                glm::ivec3 index(x, y, z);

                for (uint32_t frame = 0; frame < frameCount; frame++) {
                    uint32_t nextFrame = (frame + 1) % frameCount;

                    std::optional<glm::vec3> first = getVoxel(frames[frame], index);
                    std::optional<glm::vec3> second = getVoxel(frames[nextFrame], index);

                    auto mod = Modification::getDiff(first, second);
                    if (mod.has_value()) {
                        animation[frame].insert({ index, mod.value() });
                    }
                }
            }
        }
    }

    return animation;
}
