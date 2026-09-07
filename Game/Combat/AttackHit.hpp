#ifndef ATTACK_HIT_HPP_
#define ATTACK_HIT_HPP_

#include "Math/Vector3.hpp"

// 攻撃側で倍率を適用した結果を渡す。受け手はプレイヤーの速度を知らなくてよい。
struct AttackHit {
    float damage = 0.0f;
    Vector3 knockbackVelocity{};
};

#endif // ATTACK_HIT_HPP_
