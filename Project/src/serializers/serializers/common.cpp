#include "common.hpp"
#include "modification/diff.hpp"
#include "modification/mod_type.hpp"

#include "as_proto/general.pb.h"

namespace Serializers {

std::vector<uint8_t> vectorFromStream(std::istream& stream)
{
    stream.seekg(0, std::ios::end);

    size_t fileSize = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(fileSize);
    stream.read((char*)data.data(), fileSize);

    return data;
}

void writeHeader(
    ASProto::Header* header, glm::uvec3 dimensions, size_t voxelCount, size_t nodeCount)
{
    header->mutable_dimensions()->set_x(dimensions.x);
    header->mutable_dimensions()->set_y(dimensions.y);
    header->mutable_dimensions()->set_z(dimensions.z);

    header->set_voxelcount(voxelCount);
    header->set_nodecount(nodeCount);
}

SerialInfo readHeader(const ASProto::Header& header)
{
    return SerialInfo {
        .dimensions = glm::uvec3 { header.dimensions().x(), header.dimensions().y(),
                                  header.dimensions().z(), },
        .voxels = header.voxelcount(),
        .nodes = header.nodecount(),
    };
}

void writeDiff(ASProto::AnimationDiff* diff, glm::ivec3 position, Modification::DiffType diffType)
{
    diff->set_type(static_cast<uint32_t>(diffType.first));

    diff->mutable_position()->set_x(position.x);
    diff->mutable_position()->set_y(position.y);
    diff->mutable_position()->set_z(position.z);

    diff->mutable_colour()->set_r(diffType.second.r);
    diff->mutable_colour()->set_g(diffType.second.g);
    diff->mutable_colour()->set_b(diffType.second.b);
}

std::pair<glm::ivec3, Modification::DiffType> readDiff(const ASProto::AnimationDiff& diff)
{
    glm::ivec3 index = {
        diff.position().x(),
        diff.position().y(),
        diff.position().z(),
    };

    Modification::Type type = static_cast<Modification::Type>(diff.type());
    glm::vec3 colour = {
        diff.colour().r(),
        diff.colour().g(),
        diff.colour().b(),
    };

    return std::make_pair(index, Modification::DiffType(type, colour));
}

void writeAnimation(ASProto::Animation* animation, const Modification::AnimationFrames& frames)
{
    for (const auto& frame : frames) {
        ASProto::AnimationFrames* frames = animation->mutable_frames()->Add();
        for (const auto& diff : frame) {
            ASProto::AnimationDiff* protoDiff = frames->mutable_diffs()->Add();
            writeDiff(protoDiff, diff.first, diff.second);
        }
    }
}

Modification::AnimationFrames readAnimation(const ASProto::Animation& animation)
{
    Modification::AnimationFrames frames;

    uint32_t frameCount = animation.frames_size();
    frames.resize(frameCount);

    for (uint32_t frame = 0; frame < frameCount; frame++) {

        uint32_t diffs = animation.frames().at(frame).diffs_size();
        for (uint32_t diff = 0; diff < diffs; diff++) {
            ASProto::AnimationDiff animDiff = animation.frames().at(frame).diffs().at(diff);

            glm::ivec3 index;
            Modification::DiffType diffType;
            std::tie(index, diffType) = readDiff(animDiff);

            frames[frame].insert({ index, diffType });
        }
    }

    return frames;
}

}
