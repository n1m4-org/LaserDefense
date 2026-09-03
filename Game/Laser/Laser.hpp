#ifndef LASER_HPP_
#define LASER_HPP_

#include <memory>
#include "Combat/AttackHit.hpp"

#include "Math/Vector3.hpp"
#include "src/ParticleSystem/Emitter/Emitter.hpp"

class GameObject;
class Line;
class ParticleSystem;
namespace Collision {
    class Collider;
}

class Laser final {
    static constexpr float kDefaultLineHeight = 0.5f;
    static constexpr float kDefaultColliderRadius = 0.1f;

    struct Endpoint {
        const GameObject* object = nullptr;
        float lineHeight = kDefaultLineHeight;
    };

    struct BeamState {
        Vector3 start{};
        Vector3 end{};
    };

    std::unique_ptr<Collision::Collider> collider_;
    Endpoint start_{};
    Endpoint target_{};
    float colliderRadius_ = kDefaultColliderRadius;
    float attackPower_ = 5.0f;
    float knockbackPower_ = 18.0f;
    float damageMultiplier_ = 1.0f;
    float knockbackMultiplier_ = 1.0f;
    GESTD::ReferencePtr<ParticleSystem> particleSystem_;
    std::shared_ptr<BeamState> beamState_;
    EmitterHandle beamHandle_;

public:
    Laser();
    ~Laser();

    void Initialize(GESTD::ReferencePtr<ParticleSystem> _particleSystem);
    void Update();
    void Draw() const;

    void SetStart(const GameObject* _object, float _lineHeight = kDefaultLineHeight);
    void SetTarget(const GameObject* _object, float _lineHeight = kDefaultLineHeight);
    void ClearTarget();
    const GameObject* GetConnectedTarget() const;
    void SetColliderRadius(float _radius);
    void SetAttackMultipliers(float _damage, float _knockback);
    AttackHit GetAttackHit() const;

private:
    void LoadConfig();
    void InitializeBeamEffect();
    void UpdateBeamEffect(const Vector3& _start, const Vector3& _end);
    void StopBeamEffect();
};

#endif // LASER_HPP_
