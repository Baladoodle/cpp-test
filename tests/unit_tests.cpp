#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/MathUtils.hpp"
#include "../src/Chunk.hpp"
#include "../src/Block.hpp"
#include "../src/ChunkManager.hpp"

void testFloorDiv() {
    assert(floorDiv(0, 32) == 0);
    assert(floorDiv(31, 32) == 0);
    assert(floorDiv(32, 32) == 1);
    assert(floorDiv(35, 32) == 1);
    assert(floorDiv(-1, 32) == -1);
    assert(floorDiv(-32, 32) == -1);
    assert(floorDiv(-33, 32) == -2);
    std::cout << "[PASS] testFloorDiv\n";
}

void testCoordinateMapping() {
    assert(worldToLocalVoxel(0, 32) == 0);
    assert(worldToLocalVoxel(31, 32) == 31);
    assert(worldToLocalVoxel(32, 32) == 0);
    assert(worldToLocalVoxel(-1, 32) == 31);
    assert(worldToLocalVoxel(-32, 32) == 0);
    assert(worldToLocalVoxel(-33, 32) == 31);
    std::cout << "[PASS] testCoordinateMapping\n";
}

void testLightPacking() {
    uint8_t r = 12, g = 5, b = 15, sky = 8;
    uint16_t packed = packLight(r, g, b, sky);
    (void)packed;
    assert(getLightR(packed) == r);
    assert(getLightG(packed) == g);
    assert(getLightB(packed) == b);
    assert(getLightSky(packed) == sky);

    uint16_t zeroLight = packLight(0, 0, 0, 0);
    (void)zeroLight;
    assert(getLightR(zeroLight) == 0);
    assert(getLightSky(zeroLight) == 0);

    uint16_t maxLight = packLight(15, 15, 15, 15);
    (void)maxLight;
    assert(getLightR(maxLight) == 15);
    assert(getLightSky(maxLight) == 15);
    std::cout << "[PASS] testLightPacking\n";
}

void testBlockProperties() {
    assert(getBlockInfo(BlockType::AIR).isSolid == false);
    assert(getBlockInfo(BlockType::GRASS).isSolid == true);
    assert(getBlockInfo(BlockType::WATER).isTransparent == true);
    assert(getBlockInfo(BlockType::STONE).isOpaque == true);
    std::cout << "[PASS] testBlockProperties\n";
}
void testLodConfiguration() {
    assert(ChunkManager::NUM_LODS == 7);
    Chunk c0(IVec3(0, 0, 0), 0);
    assert(c0.scale == 1);
    assert(c0.worldSize == 32);

    Chunk c5(IVec3(0, 0, 0), 5);
    assert(c5.scale == 32);
    assert(c5.worldSize == 1024);

    Chunk c6(IVec3(0, 0, 0), 6);
    assert(c6.scale == 64);
    assert(c6.worldSize == 2048);
    std::cout << "[PASS] testLodConfiguration\n";
}

int main() {
    std::cout << "Running unit tests...\n";
    testFloorDiv();
    testCoordinateMapping();
    testLightPacking();
    testBlockProperties();
    testLodConfiguration();
    std::cout << "All unit tests passed successfully!\n";
    return 0;
}
