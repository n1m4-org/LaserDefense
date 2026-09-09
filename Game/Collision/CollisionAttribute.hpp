#ifndef COLLISION_ATTRIBUTE_HPP_
#define COLLISION_ATTRIBUTE_HPP_

#include <cstdint>

namespace CollisionAttribute {
    inline constexpr uint32_t Enemy = 1u << 0;
    inline constexpr uint32_t Tower = 1u << 1;
    inline constexpr uint32_t Laser = 1u << 2;
    inline constexpr uint32_t Player = 1u << 3;
    inline constexpr uint32_t Floor = 1u << 4;
}

#endif // COLLISION_ATTRIBUTE_HPP_
