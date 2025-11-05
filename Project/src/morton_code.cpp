#include "morton_code.hpp"

// https://forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/
inline uint64_t splitBy3(uint32_t a)
{
    uint64_t x = a % 0x1FFFFF;
    x = (x | x << 32) & 0x1f00000000ffff;
    x = (x | x << 16) & 0x1f0000ff0000ff;
    x = (x | x << 8) & 0x100f00f00f00f00f;
    x = (x | x << 4) & 0x10c30c30c30c30c3;
    x = (x | x << 2) & 0x1249249249249249;
    return x;
}

inline uint32_t combineBy3(uint64_t a)
{
    a &= 0x1249249249249249;
    a = (a ^ (a >> 2)) & 0x30c30c30c30c30c3;
    a = (a ^ (a >> 4)) & 0xf00f00f00f00f00f;
    a = (a ^ (a >> 8)) & 0x00ff0000ff0000ff;
    a = (a ^ (a >> 16)) & 0x00ff00000000ffff;
    a = (a ^ (a >> 32)) & 0x00000000001fffff;

    return (uint32_t)a;
}

namespace MortonCode {
uint64_t encode(glm::uvec3 index)
{
    uint32_t x = splitBy3(index.x);
    uint32_t y = splitBy3(index.y);
    uint32_t z = splitBy3(index.z);

    return x | y << 2 | z << 1;
}

glm::uvec3 decode(uint64_t code)
{
    uint32_t x = combineBy3(code);
    uint32_t y = combineBy3(code >> 2);
    uint32_t z = combineBy3(code >> 1);

    return glm::uvec3(x, y, z);
}
}
