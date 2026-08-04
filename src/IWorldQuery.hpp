#ifndef IWORLD_QUERY_HPP
#define IWORLD_QUERY_HPP

#include "MathUtils.hpp"
#include "Block.hpp"
#include <cstdint>

class IWorldQuery {
public:
    virtual ~IWorldQuery() = default;
    virtual bool isBlockSolidAt(int64_t wx, int64_t wy, int64_t wz) const = 0;
    virtual BlockType getBlockAt(const IVec3& worldPos) const = 0;
};

#endif // IWORLD_QUERY_HPP
