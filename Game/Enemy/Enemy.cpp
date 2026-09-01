#define NOMINMAX

#include "Enemy.hpp"

#include <algorithm>

#include "Math/Easing.hpp"
#include "Collision/CollisionAttribute.hpp"

namespace {
    constexpr float FULL_ROTATION = 6.2831853f;
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

void Enemy::Kill() {
    if (state_ == State::Death) {
        return;
    }
    state_ = State::Death;
    deathAnimationTime_ = 0.0f;
    deathAnimationFinished_ = false;
    SetVelocity({});
    if (collider_) {
        collider_->Disable();
    }
}

void Enemy::Initialize() {
    state_ = State::Spawn;
    spawnAnimationTime_ = 0.0f;
    deathAnimationTime_ = 0.0f;
    deathAnimationFinished_ = false;
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
    if ((_other->GetAttribute() & CollisionAttribute::Tower) == 0u) {
        return;
    }

    Kill();
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
}
