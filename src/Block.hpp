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
    BLOCK_TALL_GRASS = 13,
    BLOCK_TALL_GRASS_TOP = 14,
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
    uint8_t lightR;
    uint8_t lightG;
    uint8_t lightB;
    uint8_t topTex;
    uint8_t sideTex;
    uint8_t bottomTex;
};

inline const BlockInfo& getBlockInfo(uint8_t type) {
    static const BlockInfo infos[BLOCK_COUNT] = {
        // name, solid, transparent, lightR, lightG, lightB, topTex, sideTex, bottomTex
        { "Air",            false, true,  0,  0,  0,  0,  0,  0 },
        { "Grass",          true,  false, 0,  0,  0,  0,  1,  2 }, // 0=GrassTop, 1=GrassSide, 2=Dirt
        { "Dirt",           true,  false, 0,  0,  0,  2,  2,  2 },
        { "Stone",          true,  false, 0,  0,  0,  3,  3,  3 },
        { "Glow Crystal",   true,  false, 15, 13, 6,  4,  4,  4 }, // Warm Golden Glow
        { "Oak Log",        true,  false, 0,  0,  0,  6,  5,  6 }, // 5=LogSide, 6=LogTop
        { "Leaves",         true,  true,  0,  0,  0,  7,  7,  7 },
        { "Sand",           true,  false, 0,  0,  0,  8,  8,  8 },
        { "Sky Quartz",     true,  false, 4, 12, 15,  9,  9,  9 }, // Cyan Glow
        { "Water",          false, true,  0,  0,  0, 10, 10, 10 },
        { "Light Oak Leaves",true,  true,  0,  0,  0, 13, 13, 13 },
        { "Dark Oak Leaves", true,  true,  0,  0,  0, 19, 19, 19 },
        { "Warm Oak Leaves", true,  true,  0,  0,  0, 23, 23, 23 },
        { "Tall Grass",      false, true,  0,  0,  0, 29, 29, 29 },
        { "Tall Grass Top",  false, true,  0,  0,  0, 32, 32, 32 }
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

// Dummy leaf art uses several masks per palette. Keep the mapping here so
// replacing the procedural atlas with real assets later does not touch mesh
// generation or worldgen.
inline uint8_t getLeafTextureIndex(uint8_t blockType, uint8_t variant) {
    static constexpr uint8_t leafTiles[4][6] = {
        { 7, 38, 39, 40, 41, 42 },
        { 13, 43, 44, 45, 46, 47 },
        { 19, 48, 49, 50, 51, 52 },
        { 23, 53, 54, 55, 56, 57 }
    };

    int palette = 0;
    if (blockType == BLOCK_LEAVES_LIGHT) palette = 1;
    else if (blockType == BLOCK_LEAVES_DARK) palette = 2;
    else if (blockType == BLOCK_LEAVES_WARM) palette = 3;
    return leafTiles[palette][variant % 6];
}

#endif // BLOCK_HPP
