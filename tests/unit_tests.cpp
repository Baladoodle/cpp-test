#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include "../src/MathUtils.hpp"
#include "../src/Chunk.hpp"
#include "../src/Block.hpp"
#include "../src/ChunkManager.hpp"

void requireTest(bool condition) {
    if (!condition) std::abort();
}

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
    assert(getBlockInfo(BLOCK_AIR).isSolid == false);
    assert(getBlockInfo(BLOCK_GRASS).isSolid == true);
    assert(getBlockInfo(BLOCK_WATER).isTransparent == true);
    assert(getBlockInfo(BLOCK_STONE).isSolid == true);
    std::cout << "[PASS] testBlockProperties\n";
}

void testRareTallMountains() {
    int activeProfiles = 0;
    int samples = 0;
    float tallest = 0.0f;
    WorldGen::MountainProfile tallestProfile;
    bool foundTallProfile = false;

    for (int64_t x = -16000; x <= 16000; x += 256) {
        for (int64_t z = -16000; z <= 16000; z += 256) {
            ++samples;
            WorldGen::MountainProfile profile;
            if (!WorldGen::getMountainProfile(x, z, profile)) continue;
            ++activeProfiles;
            if (profile.height > tallest) {
                tallest = profile.height;
                tallestProfile = profile;
            }
            if (profile.height >= 700.0f) foundTallProfile = true;
        }
    }

    requireTest(activeProfiles > 0);
    requireTest(activeProfiles < samples / 3);
    requireTest(foundTallProfile);
    requireTest(tallest <= 1000.01f);

    int64_t probeX = 0;
    int64_t probeZ = 0;
    for (int64_t x = -16000; x <= 16000 && probeX == 0 && probeZ == 0; x += 256) {
        for (int64_t z = -16000; z <= 16000; z += 256) {
            WorldGen::MountainProfile profile;
            if (WorldGen::getMountainProfile(x, z, profile) &&
                profile.height >= 700.0f) {
                probeX = x;
                probeZ = z;
                break;
            }
        }
    }

    WorldGen::MountainProfile probe;
    requireTest(WorldGen::getMountainProfile(probeX, probeZ, probe));
    int64_t baseY = static_cast<int64_t>(probe.baseY);
    int64_t surfaceY = static_cast<int64_t>(std::ceil(probe.topY)) - 1;
    requireTest(WorldGen::getMountainDensity(probeX, baseY + 32, probeZ, 1) > 0.0f);
    requireTest(WorldGen::getMountainDensity(probeX, surfaceY + 1, probeZ, 1) == 0.0f);
    requireTest(WorldGen::isMountainColdSurface(probeX, surfaceY, probeZ));
    requireTest(WorldGen::getBlockAt(probeX, surfaceY, probeZ, 1) == BLOCK_SKY_QUARTZ);
    int64_t scannedSurfaceY = WorldGen::getSurfaceYAt(probeX, probeZ, -1000, 700);
    requireTest(scannedSurfaceY >= surfaceY);
    Chunk mountainChunk(
        IVec3(
            floorDiv(probeX, CHUNK_SIZE),
            floorDiv(surfaceY, CHUNK_SIZE),
            floorDiv(probeZ, CHUNK_SIZE)
        ),
        0
    );
    WorldGenerator::generateVoxelData(mountainChunk);
    requireTest(mountainChunk.isGenerated.load());
    requireTest(!mountainChunk.isEmpty);
    int localX = static_cast<int>(probeX - mountainChunk.worldMin.x);
    int localY = static_cast<int>(surfaceY - mountainChunk.worldMin.y);
    int localZ = static_cast<int>(probeZ - mountainChunk.worldMin.z);
    requireTest(mountainChunk.getBlock(localX, localY, localZ) == BLOCK_SKY_QUARTZ);
    requireTest(tallestProfile.height == tallest);
    std::cout << "[PASS] testRareTallMountains\n";
}

int main() {
    std::cout << "Running unit tests...\n";
    testFloorDiv();
    testCoordinateMapping();
    testLightPacking();
    testBlockProperties();
    testRareTallMountains();
    std::cout << "All unit tests passed successfully!\n";
    return 0;
}
