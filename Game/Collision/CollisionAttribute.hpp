#ifndef COLLISION_ATTRIBUTE_HPP_
#define COLLISION_ATTRIBUTE_HPP_

#include <cstdint>

namespace CollisionAttribute {
    inline constexpr uint32_t Enemy = 1u << 0;
    inline constexpr uint32_t Tower = 1u << 1;
    inline constexpr uint32_t Laser = 1u << 2;
}

#endif // COLLISION_ATTRIBUTE_HPP_
