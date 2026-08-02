#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <cstdint>

enum BlockType : uint8_t {
    BLOCK_AIR = 0,
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_STONE = 3,
    BLOCK_GLOW_CRYSTAL = 4,
    BLOCK_OAK_LOG = 5,
    BLOCK_LEAVES = 6,
    BLOCK_SAND = 7,
    BLOCK_SKY_QUARTZ = 8,
    BLOCK_WATER = 9,
    BLOCK_LEAVES_LIGHT = 10,
    BLOCK_LEAVES_DARK = 11,
    BLOCK_LEAVES_WARM = 12,
    BLOCK_COUNT
};

enum Direction : uint8_t {
    DIR_POS_X = 0,
    DIR_NEG_X = 1,
    DIR_POS_Y = 2,
    DIR_NEG_Y = 3,
    DIR_POS_Z = 4,
    DIR_NEG_Z = 5
};

struct BlockInfo {
    const char* name;
    bool isSolid;
    bool isTransparent;
    uint8_t lightEmission;
    uint8_t topTex;
    uint8_t sideTex;
    uint8_t bottomTex;
};

inline const BlockInfo& getBlockInfo(uint8_t type) {
    static const BlockInfo infos[BLOCK_COUNT] = {
        // name, solid, transparent, lightEmission, topTex, sideTex, bottomTex
        { "Air",            false, true,  0,  0,  0,  0 },
        { "Grass",          true,  false, 0,  0,  1,  2 }, // 0=GrassTop, 1=GrassSide, 2=Dirt
        { "Dirt",           true,  false, 0,  2,  2,  2 },
        { "Stone",          true,  false, 0,  3,  3,  3 },
        { "Glow Crystal",   true,  false, 15, 4,  4,  4 }, // Emissive light level 15
        { "Oak Log",        true,  false, 0,  6,  5,  6 }, // 5=LogSide, 6=LogTop
        { "Leaves",         true,  true,  0,  7,  7,  7 },
        { "Sand",           true,  false, 0,  8,  8,  8 },
        { "Sky Quartz",     true,  false, 8,  9,  9,  9 },
        { "Water",          false, true,  0, 10, 10, 10 },
        { "Light Oak Leaves",true,  true,  0, 13, 13, 13 },
        { "Dark Oak Leaves", true,  true,  0, 19, 19, 19 },
        { "Warm Oak Leaves", true,  true,  0, 23, 23, 23 }
    };
    if (type >= BLOCK_COUNT) return infos[0];
    return infos[type];
}

inline bool isAnyLeaf(uint8_t type) {
    return type == BLOCK_LEAVES || type == BLOCK_LEAVES_LIGHT ||
           type == BLOCK_LEAVES_DARK || type == BLOCK_LEAVES_WARM;
}

inline uint8_t getBlockTextureIndex(uint8_t blockType, uint8_t direction) {
    const BlockInfo& info = getBlockInfo(blockType);
    if (direction == DIR_POS_Y) return info.topTex;
    if (direction == DIR_NEG_Y) return info.bottomTex;
    return info.sideTex;
}

#endif // BLOCK_HPP
