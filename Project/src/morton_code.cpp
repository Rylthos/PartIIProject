#include "morton_code.hpp"

// https://forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/
inline uint64_t splitBy3(uint32_t a)
{
    uint64_t x = a % 0x1FFFFF;
    // x = 0000000000000000000000000000000000000000000ABCDEFGHIJKLMNOPQRSTU
    x = (x | x << 32) & 0x1f00000000ffff;
    // x = 00000000000ABCDE00000000000000000000000000000000FGHIJKLMNOPQRSTU
    x = (x | x << 16) & 0x1f0000ff0000ff;
    // x = 00000000000ABCDE0000000000000000FGHIJKLM0000000000000000NOPQRSTU
    x = (x | x << 8) & 0x100f00f00f00f00f;
    // x = 000A00000000BCDE00000000FGHI00000000JKLM00000000NOPQ00000000RSTU
    x = (x | x << 4) & 0x10c30c30c30c30c3;
    // x = 000A0000BC0000DE0000FG0000HI0000JK0000LM0000NO0000PQ0000RS0000TU
    x = (x | x << 2) & 0x1249249249249249;
    // x = 000A00B00C00D00E00F00G00H00I00J00K00L00M00N00O00P00Q00R00S00T00U
    return x;
}

inline uint32_t combineBy3(uint64_t a)
{
    a &= 0x1249249249249249;
    // a = 000A00B00C00D00E00F00G00H00I00J00K00L00M00N00O00P00Q00R00S00T00U
    a = (a ^ (a >> 2)) & 0x30c30c30c30c30c3;
    // a = 000A0000BC0000DE0000FG0000HI0000JK0000LM0000NO0000PQ0000RS0000TU
    a = (a ^ (a >> 4)) & 0xf00f00f00f00f00f;
    // a = 000A00000000BCDE00000000FGHI00000000JKLM00000000NOPQ00000000RSTU
    a = (a ^ (a >> 8)) & 0x00ff0000ff0000ff;
    // a = 00000000000ABCDE0000000000000000FGHIJKLM0000000000000000NOPQRSTU
    a = (a ^ (a >> 16)) & 0x00ff00000000ffff;
    // a = 00000000000ABCDE00000000000000000000000000000000FGHIJKLMNOPQRSTU
    a = (a ^ (a >> 32)) & 0x00000000001fffff;
    // a = 0000000000000000000000000000000000000000000ABCDEFGHIJKLMNOPQRSTU
    return (uint32_t)a;
}

inline uint64_t splitBy2x3(uint32_t a)
{
    uint64_t x = a % 0xFFFFF;
    // x = 00000000000000000000000000000000000000000000BCDEFGHIJKLMNOPQRSTU
    x = (x | x << 32) & 0xf00000000ffff;
    // x = 000000000000BCDE00000000000000000000000000000000FGHIJKLMNOPQRSTU
    x = (x | x << 16) & 0xf0000ff0000ff;
    // x = 000000000000BCDE0000000000000000FGHIJKLM0000000000000000NOPQRSTU
    x = (x | x << 8) & 0xf00f00f00f00f;
    // x = 000000000000BCDE00000000FGHI00000000JKLM00000000NOPQ00000000RSTU
    x = (x | x << 4) & 0xc30c30c30c30c3;
    // x = 00000000BC0000DE0000FG0000HI0000JK0000LM0000NO0000PQ0000RS0000TU
    return x;
}

inline uint32_t combineBy2x3(uint64_t a)
{
    a &= 0x00C30C30C30C30C3;
    // a = 00000000BC0000DE0000FG0000HI0000JK0000LM0000NO0000PQ0000RS0000TU
    a = (a ^ (a >> 4)) & 0xf00f00f00f00f00f;
    // a = 000000000000BCDE00000000FGHI00000000JKLM00000000NOPQ00000000RSTU
    a = (a ^ (a >> 8)) & 0x00ff0000ff0000ff;
    // a = 000000000000BCDE0000000000000000FGHIJKLM0000000000000000NOPQRSTU
    a = (a ^ (a >> 16)) & 0x00ff00000000ffff;
    // a = 000000000000BCDE00000000000000000000000000000000FGHIJKLMNOPQRSTU
    a = (a ^ (a >> 32)) & 0x00000000001fffff;
    // a = 00000000000000000000000000000000000000000000BCDEFGHIJKLMNOPQRSTU
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

uint64_t encode2(glm::uvec3 index)
{
    uint32_t x = splitBy3(index.x);
    uint32_t y = splitBy3(index.y);
    uint32_t z = splitBy3(index.z);

    return x | y << 4 | z << 2;
}

glm::uvec3 decode2(uint64_t code)
{
    uint32_t x = combineBy3(code);
    uint32_t y = combineBy3(code >> 4);
    uint32_t z = combineBy3(code >> 2);

    return glm::uvec3(x, y, z);
}
}
