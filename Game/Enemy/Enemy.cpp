#define NOMINMAX

#include "Enemy.hpp"

#include <algorithm>
#include <cmath>

#include "Math/Easing.hpp"
#include "Collision/CollisionAttribute.hpp"
#include "Laser/Laser.hpp"
#include "Camera/Controller/CameraController.hpp"
#include "Math/MathUtils.hpp"
#include "Pattern/Singleton.hpp"
#include "Screen/Screen.hpp"

namespace {
    constexpr float FULL_ROTATION = 6.2831853f;
}

void Enemy::SetHealth(float _maxHp, float _knockbackBrake) {
    maxHp_ = std::isfinite(_maxHp) ? std::max(_maxHp, 0.0001f) : 10.0f;
    hp_ = maxHp_;
    knockbackBrake_ = std::isfinite(_knockbackBrake) ? std::max(_knockbackBrake, 0.0f) : 5.0f;
}

void Enemy::TakeDamage(const AttackHit& _hit) {
    if (!active_ || !IsAlive() || !std::isfinite(_hit.damage) || _hit.damage <= 0.0f) return;
    hp_ = std::max(0.0f, hp_ - _hit.damage);
    if (std::isfinite(_hit.knockbackVelocity.x) && std::isfinite(_hit.knockbackVelocity.z)) {
        // 再ヒット時は今回の攻撃方向で上書きし、無制限な速度の蓄積を避ける。
        knockbackVelocity_ = {_hit.knockbackVelocity.x, 0.0f, _hit.knockbackVelocity.z};
    }
    if (hp_ <= 0.0f) Kill(true);
}

void Enemy::SetAppearance(const std::string& _modelName, const Vector3& _scale,
    const Vector3& _offset, const Vector4& _color) {
    modelName_ = _modelName;
    modelScale_ = _scale;
    modelOffset_ = _offset;
    modelColor_ = _color;
}

void Enemy::SetMovement(const Vector3& _targetPosition, float _moveSpeed) {
    targetPosition_ = { _targetPosition.x, 0.0f, _targetPosition.z };
    moveSpeed_ = _moveSpeed;
}

void Enemy::SetSpawnAnimation(float _duration, float _startScale,
    float _rotations, bool _moveDuringAnimation) {
    spawnAnimationDuration_ = _duration;
    spawnStartScale_ = _startScale;
    spawnRotations_ = _rotations;
    moveDuringSpawnAnimation_ = _moveDuringAnimation;
}

void Enemy::SetDeathAnimation(float _duration, float _peakScale,
    float _endScale, float _expandRatio) {
    deathAnimationDuration_ = _duration;
    deathPeakScale_ = _peakScale;
    deathEndScale_ = _endScale;
    deathExpandRatio_ = _expandRatio;
}

int32_t Enemy::TakeScoreReward() {
    if (!scorePending_) {
        return 0;
    }
    // 回収済みにしてから返すことで、同じ敵から二重にスコアが入らないようにする
    scorePending_ = false;
    return scoreValue_;
}

void Enemy::Kill(bool _awardsScore) {
    if (state_ == State::Death) {
        return;
    }
    state_ = State::Death;
    hp_ = 0.0f;
    // スコア加算対象の撃破なら、回収待ち状態にする
    scorePending_ = _awardsScore;
    deathAnimationTime_ = 0.0f;
    deathAnimationFinished_ = false;
    SetVelocity({});
    if (collider_) {
        collider_->Disable();
    }
}

void Enemy::Initialize() {
    hp_ = maxHp_;
    knockbackVelocity_ = {};
    hpBarBackground_.Initialize("white_x16.png");
    hpBarBackground_.SetAnchorPoint({0.0f, 0.0f});
    hpBarBackground_.SetColor({0.1f, 0.1f, 0.1f, 1.0f});
    hpBarFill_.Initialize("white_x16.png");
    hpBarFill_.SetAnchorPoint({0.0f, 0.0f});
    hpBarFill_.SetColor({0.2f, 1.0f, 0.2f, 1.0f});
    state_ = State::Spawn;
    spawnAnimationTime_ = 0.0f;
    deathAnimationTime_ = 0.0f;
    deathAnimationFinished_ = false;
    scorePending_ = false;
    SetModel(modelName_);
    SetPosition({ 0.0f, 0.0f, 0.0f });
    SetScale(modelScale_ * spawnStartScale_);
    SetRotation({});
    model_->SetColor(modelColor_);

    collider_ = std::make_unique<Collision::Collider>();
    collider_->SetName("Enemy")
        ->SetType(Collision::Type::AABB)
        ->SetOwner(this)
        ->AddAttribute(CollisionAttribute::Enemy)
        ->AddIgnore(CollisionAttribute::Enemy)
        ->SetEvent(Collision::EventType::Trigger, [this](const Collision::Collider* _other) {
        OnCollisionTrigger(_other);
            })
        ->Enable();
}

void Enemy::Update(float _deltaTime) {
    offset_ = modelOffset_;

    switch (state_) {
        case State::Spawn:
            UpdateSpawnAnimation(_deltaTime);

            if (moveDuringSpawnAnimation_ && state_ == State::Spawn) {
                UpdateMovement(_deltaTime);
            }

            if (state_ == State::Spawn && !IsSpawnAnimationPlaying()) {
                state_ = State::Move;
            }
            break;

        case State::Move:
            UpdateMovement(_deltaTime);
            break;

        case State::Death:
            UpdateDeathAnimation(_deltaTime);
            break;
    }

    // 通常移動とは別の速度として加算。死亡演出中も吹き飛びを継続する。
    if (std::isfinite(_deltaTime) && _deltaTime > 0.0f) {
        const float dt = std::min(_deltaTime, 0.1f);
        const float decay = std::exp(-knockbackBrake_ * dt);
        const float travel = knockbackBrake_ > 0.0001f ? (1.0f - decay) / knockbackBrake_ : dt;
        position_ += knockbackVelocity_ * travel;
        knockbackVelocity_ = knockbackVelocity_ * decay;
    }
    UpdateCollider();
    UpdateModel();
}

void Enemy::UpdateCollider() {
    if (!collider_) {
        return;
    }

    collider_->SetTranslate(position_ + offset_ + colliderOffset_);
    collider_->SetSize(scale_ * 2.0f);
}

void Enemy::UpdateMovement(float _deltaTime) {
    const Vector3 toTarget = targetPosition_ - position_;
    const float distance = toTarget.Length();
    const float moveDistance = moveSpeed_ * _deltaTime;

    if (distance <= moveDistance) {
        SetPosition(targetPosition_);
        SetVelocity({});
    } else {
        SetVelocity(toTarget.Normalize() * moveSpeed_);
        ApplyVelocity(_deltaTime);
    }

}

void Enemy::OnCollisionTrigger(const Collision::Collider* _other) {
    if (!_other) {
        return;
    }
    if ((_other->GetAttribute() & CollisionAttribute::Laser) != 0u) {
        // LaserカテゴリのOwnerはLaser。Triggerなので接触開始時のみダメージ。
        if (const auto* laser = static_cast<const Laser*>(_other->GetOwner())) {
            TakeDamage(laser->GetAttackHit());
        }
        return;
    }
    if ((_other->GetAttribute() & CollisionAttribute::Tower) != 0u) {
        Kill(awardsScoreOnTowerHit_);
    }

}

void Enemy::UpdateSpawnAnimation(float _deltaTime) {
    if (spawnAnimationDuration_ <= 0.0f) {
        SetScale(modelScale_);
        SetRotation({});
        offset_ = modelOffset_;
        return;
    }

    spawnAnimationTime_ = std::min(
        spawnAnimationTime_ + _deltaTime,
        spawnAnimationDuration_);

    const float t = spawnAnimationTime_ / spawnAnimationDuration_;
    const Vector3 startScale = modelScale_ * spawnStartScale_;
    const float rotationAngle = FULL_ROTATION * spawnRotations_;

    SetScale(Ease::Out::Cubic(startScale, modelScale_, t));
    SetRotation(Ease::InOut::Cubic(Vector3{}, { 0.0f, rotationAngle, 0.0f }, t));
    offset_ = Ease::Out::Cubic(modelOffset_ * spawnStartScale_, modelOffset_, t);

    if (!IsSpawnAnimationPlaying()) {
        SetScale(modelScale_);
        SetRotation({});
        offset_ = modelOffset_;
    }
}

bool Enemy::IsSpawnAnimationPlaying() const {
    return spawnAnimationTime_ < spawnAnimationDuration_;
}

void Enemy::UpdateDeathAnimation(float _deltaTime) {
    if (deathAnimationDuration_ <= 0.0f) {
        SetScale(modelScale_ * deathEndScale_);
        offset_ = modelOffset_ * deathEndScale_;
        deathAnimationFinished_ = true;
        return;
    }

    deathAnimationTime_ = std::min(
        deathAnimationTime_ + _deltaTime,
        deathAnimationDuration_);

    const float t = deathAnimationTime_ / deathAnimationDuration_;
    const Vector3 peakScale = modelScale_ * deathPeakScale_;
    const Vector3 peakOffset = modelOffset_ * deathPeakScale_;
    const Vector3 endScale = modelScale_ * deathEndScale_;
    const Vector3 endOffset = modelOffset_ * deathEndScale_;

    if (t < deathExpandRatio_) {
        const float localT = t / deathExpandRatio_;
        SetScale(Ease::Out::Cubic(modelScale_, peakScale, localT));
        offset_ = Ease::Out::Cubic(modelOffset_, peakOffset, localT);
    } else {
        const float localT = (t - deathExpandRatio_) / (1.0f - deathExpandRatio_);
        SetScale(Ease::In::Cubic(peakScale, endScale, localT));
        offset_ = Ease::In::Cubic(peakOffset, endOffset, localT);
    }

    if (deathAnimationTime_ >= deathAnimationDuration_) {
        SetScale(endScale);
        offset_ = endOffset;
        deathAnimationFinished_ = true;
    }
}

void Enemy::Draw() {
    if (model_) {
        model_->Draw();
    }
    DrawHpBar();
}

void Enemy::DrawHpBar() {
    if (!active_ || !IsAlive() || hp_ >= maxHp_ || hp_ <= 0.0f) return;
    const auto camera = Singleton<CameraController>::GetInstance()->GetActive();
    const auto screen = Singleton<Screen>::GetInstance();
    if (!camera || screen->Width() <= 0.0f || screen->Height() <= 0.0f) return;
    const Vector3 head = GetTransform().translate + Vector3{0.0f, scale_.y + 0.3f, 0.0f};
    const Vector3 projected = MathUtils::Matrix::Transform(head, camera->GetViewProjection());
    if (!std::isfinite(projected.x) || !std::isfinite(projected.y)
        || !std::isfinite(projected.z) || projected.z < 0.0f || projected.z > 1.0f
        || std::abs(projected.x) > 1.0f || std::abs(projected.y) > 1.0f) return;
    const float x = (projected.x + 1.0f) * 0.5f * screen->Width() - 16.0f;
    const float y = (1.0f - projected.y) * 0.5f * screen->Height() - 8.0f;
    hpBarBackground_.SetPosition({x, y});
    hpBarBackground_.SetSize({32.0f, 5.0f});
    hpBarFill_.SetPosition({x + 1.0f, y + 1.0f});
    hpBarFill_.SetSize({30.0f * std::clamp(hp_ / maxHp_, 0.0f, 1.0f), 3.0f});
    hpBarBackground_.Update();
    hpBarFill_.Update();
    hpBarBackground_.Draw();
    hpBarFill_.Draw();
}
