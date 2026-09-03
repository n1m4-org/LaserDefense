#define NOMINMAX
#include "Laser.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <variant>

#include "Collision/Collider.h"
#include "Collision/CollisionAttribute.hpp"
#include "GameObject/GameObject.hpp"
#include "Json/JsonParams.hpp"
#include "Pattern/Singleton.hpp"
#include "Line.hpp"
#include "Math/MathUtils.hpp"
#include "src/ParticleSystem/ParticleSystem.hpp"

namespace {
    constexpr const char* BeamTemplate = "LaserBeam";
    constexpr const char* BeamSpawnFunc = "LaserBeamSpawn";
    constexpr float BeamHalfWidth = 0.15f;
}

Laser::Laser() = default;
Laser::~Laser() {
    StopBeamEffect();
}

void Laser::Initialize(GESTD::ReferencePtr<ParticleSystem> _particleSystem) {
    StopBeamEffect();
    particleSystem_ = _particleSystem;
    LoadConfig();

    collider_ = std::make_unique<Collision::Collider>();
    collider_->SetName("Laser")
        ->SetType(Collision::Type::Capsule)
        ->SetOwner(this)
        ->AddAttribute(CollisionAttribute::Laser)
        ->AddIgnore(CollisionAttribute::Laser);
    collider_->Disable();
    InitializeBeamEffect();
}

void Laser::LoadConfig() {
    const auto json = Singleton<JsonParams>::GetInstance();
    if (!json->Load("Laser", "Laser")) return;
    const auto groups = json->GetGroups("Laser");
    const auto collider = groups.find("Collider");
    if (collider == groups.end()) return;
    const auto radius = collider->second.find("Radius");
    if (radius == collider->second.end()) return;
    if (const auto value = std::get_if<float>(&radius->second)) SetColliderRadius(*value);
    else if (const auto integer = std::get_if<int32_t>(&radius->second)) SetColliderRadius(static_cast<float>(*integer));
}

void Laser::InitializeBeamEffect() {
    if (!particleSystem_) return;

    beamState_ = std::make_shared<BeamState>();
    // 登録関数はParticleSystemに残るため、Laser自身をキャプチャしない。
    const std::weak_ptr<BeamState> state = beamState_;
    particleSystem_->RegisterSpawnFunc(BeamSpawnFunc,
        [state](const Vector3& _center, Vector3& _outPos, Vector3& _outVel) {
            const auto beam = state.lock();
            if (!beam) {
                _outPos = _center;
                _outVel = {};
                return;
            }

            const Vector3 segment = beam->end - beam->start;
            Vector3 side{ -segment.z, 0.0f, segment.x };
            side = side.Length() > 0.0001f ? side.Normalize() : Vector3{ 1.0f, 0.0f, 0.0f };
            _outPos = beam->start + segment * MathUtils::Random(0.0f, 1.0f)
                + side * MathUtils::Random(-BeamHalfWidth, BeamHalfWidth);
            _outVel = {
                MathUtils::Random(-0.3f, 0.3f),
                MathUtils::Random(1.5f, 3.0f),
                MathUtils::Random(-0.3f, 0.3f) };
        });

    ParticleSystem::EmitterConfig config;
    config.texture = "white_x16.png";
    config.frequency = 0.012f;
    // 接続が切れるまで継続。粒子自体の寿命は別途指定する。
    config.duration = std::numeric_limits<float>::max();
    config.spawnCount = 8;
    config.size = { 0.15f, 0.15f, 0.15f };
    config.particleLifetime = 0.8f;
    config.spawnFuncKey = BeamSpawnFunc;
    config.colorKeys = {
        GradientKey<Vector4>{0.0f, {1.0f, 1.0f, 1.0f, 1.0f}},
        GradientKey<Vector4>{0.3f, {0.1f, 0.55f, 1.0f, 1.0f}},
        GradientKey<Vector4>{1.0f, {0.0f, 0.05f, 0.6f, 0.0f}}
    };
    config.sizeKeys = {
        GradientKey<Vector3>{0.0f, {0.15f, 0.15f, 0.15f}},
        GradientKey<Vector3>{0.2f, {0.35f, 0.35f, 0.35f}},
        GradientKey<Vector3>{1.0f, {0.02f, 0.02f, 0.02f}}
    };

    ParticleSystem::Template beamTemplate;
    beamTemplate.emitters.push_back(config);
    particleSystem_->Register(BeamTemplate, beamTemplate, true);
}

void Laser::UpdateBeamEffect(const Vector3& _start, const Vector3& _end) {
    if (!particleSystem_ || !beamState_) return;

    beamState_->start = _start;
    beamState_->end = _end;
    const Vector3 center = (_start + _end) * 0.5f;
    if (!beamHandle_.IsValid()) {
        beamHandle_ = particleSystem_->Emit(BeamTemplate, center);
    }
    beamHandle_.SetPosition(center);
}

void Laser::StopBeamEffect() {
    beamHandle_.Stop();
    beamHandle_ = {};
}

void Laser::Update() {

    bool connected = false;

    if (start_.object && start_.object->IsActive()) {
        const Vector3 startPosition = start_.object->GetPosition() + Vector3{ 0.0f, start_.lineHeight, 0.0f };

        if (target_.object && target_.object->IsActive()) {
            const Vector3 targetPosition =
                target_.object->GetPosition() + Vector3{ 0.0f, target_.lineHeight, 0.0f };

            collider_->SetTranslate(startPosition);
            collider_->SetSize(Collision::CapsuleShape{
                targetPosition - startPosition,
                colliderRadius_ });
            collider_->Enable();
            UpdateBeamEffect(startPosition, targetPosition);
            connected = true;
        }
    }

    if (!connected && collider_) {
        collider_->Disable();
    }
    if (!connected) {
        StopBeamEffect();
    }

}

void Laser::Draw() const {
}

void Laser::SetStart(const GameObject* _object, float _lineHeight) {
    if (start_.object != _object) StopBeamEffect();
    start_ = { _object, _lineHeight };
}

void Laser::SetTarget(const GameObject* _object, float _lineHeight) {
    if (target_.object != _object) StopBeamEffect();
    target_ = { _object, _lineHeight };
}

void Laser::ClearTarget() {
    StopBeamEffect();
    target_ = {};
    if (collider_) {
        collider_->Disable();
    }
}

void Laser::SetColliderRadius(float _radius) {
    if (!std::isfinite(_radius)) return;
    colliderRadius_ = std::max(_radius, 0.0001f);
}

const GameObject* Laser::GetConnectedTarget() const {
    return start_.object && start_.object->IsActive()
        && target_.object && target_.object->IsActive() ? target_.object : nullptr;
}
